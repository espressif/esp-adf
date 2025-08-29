/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "esp_service_mcp_trans_sdio.h"

static const char *TAG = "MCP_SDIO";

/**
 * SDIO slave peripheral is only available on certain ESP32 variants.
 * Guard the implementation with a compile-time check.
 */
#if SOC_SDIO_SLAVE_SUPPORTED

#include "driver/sdio_slave.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/**
 * @brief  Concrete SDIO transport structure
 */
typedef struct {
    esp_service_mcp_trans_t        base;  /*!< MUST be first field */
    esp_service_mcp_sdio_config_t  config;
    TaskHandle_t                   reader_task;
    SemaphoreHandle_t              write_mutex;      /*!< Protect SDIO sends */
    SemaphoreHandle_t              reader_exit_sem;  /*!< Signalled by reader task before vTaskDelete */
    bool                           running;
    uint8_t                      **recv_buffers;  /*!< Array of receive buffer pointers */
} sdio_impl_t;

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t sdio_start(esp_service_mcp_trans_t *transport);
static esp_err_t sdio_stop(esp_service_mcp_trans_t *transport);
static esp_err_t sdio_destroy(esp_service_mcp_trans_t *transport);
static esp_err_t sdio_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

static const esp_service_mcp_trans_ops_t sdio_ops = {
    .start     = sdio_start,
    .stop      = sdio_stop,
    .destroy   = sdio_destroy,
    .broadcast = sdio_broadcast,
};

/* ─── Send helper ─── */

static esp_err_t sdio_send_data(sdio_impl_t *sd, const char *data, size_t len)
{
    if (!sd->running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(sd->write_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire write mutex");
        return ESP_ERR_TIMEOUT;
    }
    if (!sd->running) {
        xSemaphoreGive(sd->write_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate a DMA-capable buffer for the send */
    uint8_t *send_buf = heap_caps_malloc(len, MALLOC_CAP_DMA);
    if (!send_buf) {
        xSemaphoreGive(sd->write_mutex);
        ESP_LOGE(TAG, "Failed to allocate DMA send buffer");
        return ESP_ERR_NO_MEM;
    }
    memcpy(send_buf, data, len);

    esp_err_t ret = sdio_slave_send_queue(send_buf, len, send_buf, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue send: %s", esp_err_to_name(ret));
        free(send_buf);
        xSemaphoreGive(sd->write_mutex);
        return ret;
    }

    /* Wait for send to complete and free the buffer */
    void *finished_arg = NULL;
    ret = sdio_slave_send_get_finished(&finished_arg, pdMS_TO_TICKS(2000));
    if (finished_arg) {
        free(finished_arg);
    }

    xSemaphoreGive(sd->write_mutex);
    return ret;
}

static void sdio_reader_task(void *arg)
{
    sdio_impl_t *sd = (sdio_impl_t *)arg;

    ESP_LOGI(TAG, "SDIO reader task started");

    while (sd->running) {
        /* Wait for data from host */
        sdio_slave_buf_handle_t recv_handle;
        uint8_t *recv_ptr = NULL;
        size_t recv_len = 0;

        esp_err_t ret = sdio_slave_recv(&recv_handle, &recv_ptr, &recv_len, pdMS_TO_TICKS(100));
        if (ret == ESP_ERR_TIMEOUT) {
            continue;
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SDIO recv failed: %s", esp_err_to_name(ret));
            continue;
        }

        if (recv_len == 0 || !recv_ptr) {
            sdio_slave_recv_load_buf(recv_handle);
            continue;
        }

        /* Ensure null termination */
        if (recv_len < sd->config.recv_buffer_size) {
            recv_ptr[recv_len] = '\0';
        } else {
            recv_ptr[sd->config.recv_buffer_size - 1] = '\0';
        }

        ESP_LOGD(TAG, "Received SDIO data (%d bytes): %s", (int)recv_len, (char *)recv_ptr);

        /* Dispatch through the abstract transport layer */
        char *response_str = NULL;
        ret = esp_service_mcp_trans_dispatch_request(&sd->base, (char *)recv_ptr, &response_str);

        /* Re-register the buffer for next receive */
        sdio_slave_recv_load_buf(recv_handle);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Request dispatch failed: %s", esp_err_to_name(ret));
            continue;
        }

        if (response_str) {
            sdio_send_data(sd, response_str, strlen(response_str) + 1);  /* +1 for null terminator */
            ESP_LOGD(TAG, "Sent response: %s", response_str);
            free(response_str);
        }
    }

    ESP_LOGI(TAG, "SDIO reader task exiting");
    xSemaphoreGive(sd->reader_exit_sem);
    vTaskDelete(NULL);
}

static esp_err_t sdio_start(esp_service_mcp_trans_t *transport)
{
    sdio_impl_t *sd = (sdio_impl_t *)transport;

    if (sd->running) {
        ESP_LOGW(TAG, "SDIO transport already running");
        return ESP_OK;
    }

    /* Initialize SDIO slave */
    sdio_slave_config_t slave_config = {
        .sending_mode = SDIO_SLAVE_SEND_STREAM,
        .send_queue_size = 4,
        .recv_buffer_size = sd->config.recv_buffer_size,
    };

    esp_err_t ret = sdio_slave_initialize(&slave_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO slave: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Allocate and register receive buffers */
    sd->recv_buffers = calloc(sd->config.recv_buffer_count, sizeof(uint8_t *));
    if (!sd->recv_buffers) {
        sdio_slave_deinit();
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < sd->config.recv_buffer_count; i++) {
        sd->recv_buffers[i] = heap_caps_malloc(sd->config.recv_buffer_size, MALLOC_CAP_DMA);
        if (!sd->recv_buffers[i]) {
            ESP_LOGE(TAG, "Failed to allocate recv buffer %d", i);
            for (int j = 0; j < i; j++) {
                free(sd->recv_buffers[j]);
            }
            free(sd->recv_buffers);
            sd->recv_buffers = NULL;
            sdio_slave_deinit();
            return ESP_ERR_NO_MEM;
        }

        sdio_slave_buf_handle_t buf_handle = sdio_slave_recv_register_buf(sd->recv_buffers[i]);
        if (!buf_handle) {
            ESP_LOGE(TAG, "Failed to register recv buffer %d", i);
            for (int j = 0; j <= i; j++) {
                free(sd->recv_buffers[j]);
            }
            free(sd->recv_buffers);
            sd->recv_buffers = NULL;
            sdio_slave_deinit();
            return ESP_FAIL;
        }
        ret = sdio_slave_recv_load_buf(buf_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load recv buffer %d: %s", i, esp_err_to_name(ret));
            for (int j = 0; j <= i; j++) {
                free(sd->recv_buffers[j]);
            }
            free(sd->recv_buffers);
            sd->recv_buffers = NULL;
            sdio_slave_deinit();
            return ret;
        }
    }

    /* Start SDIO slave */
    ret = sdio_slave_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SDIO slave: %s", esp_err_to_name(ret));
        for (int i = 0; i < sd->config.recv_buffer_count; i++) {
            free(sd->recv_buffers[i]);
        }
        free(sd->recv_buffers);
        sd->recv_buffers = NULL;
        sdio_slave_deinit();
        return ret;
    }

    sd->running = true;

    /* Create reader task */
    BaseType_t xret;
    if (sd->config.task_core >= 0) {
        xret = xTaskCreatePinnedToCore(sdio_reader_task, "mcp_sdio",
                                       sd->config.stack_size, sd,
                                       sd->config.task_priority,
                                       &sd->reader_task,
                                       sd->config.task_core);
    } else {
        xret = xTaskCreate(sdio_reader_task, "mcp_sdio",
                           sd->config.stack_size, sd,
                           sd->config.task_priority,
                           &sd->reader_task);
    }

    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reader task");
        sd->running = false;
        sdio_slave_stop();
        for (int i = 0; i < sd->config.recv_buffer_count; i++) {
            free(sd->recv_buffers[i]);
        }
        free(sd->recv_buffers);
        sd->recv_buffers = NULL;
        sdio_slave_deinit();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SDIO transport started (buf_size=%d, buf_count=%d)",
             (int)sd->config.recv_buffer_size, sd->config.recv_buffer_count);
    return ESP_OK;
}

static esp_err_t sdio_stop(esp_service_mcp_trans_t *transport)
{
    sdio_impl_t *sd = (sdio_impl_t *)transport;

    if (!sd->running) {
        return ESP_OK;
    }

    sd->running = false;

    if (sd->reader_task) {
        if (xSemaphoreTake(sd->reader_exit_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
            ESP_LOGW(TAG, "Reader task did not exit within 5s; forcing delete");
            vTaskDelete(sd->reader_task);
        }
        sd->reader_task = NULL;
    }

    sdio_slave_stop();

    if (sd->recv_buffers) {
        for (int i = 0; i < sd->config.recv_buffer_count; i++) {
            if (sd->recv_buffers[i]) {
                free(sd->recv_buffers[i]);
            }
        }
        free(sd->recv_buffers);
        sd->recv_buffers = NULL;
    }

    sdio_slave_deinit();

    ESP_LOGI(TAG, "SDIO transport stopped");
    return ESP_OK;
}

static esp_err_t sdio_destroy(esp_service_mcp_trans_t *transport)
{
    sdio_impl_t *sd = (sdio_impl_t *)transport;

    if (sd->running) {
        sdio_stop(transport);
    }

    if (sd->write_mutex) {
        vSemaphoreDelete(sd->write_mutex);
    }
    if (sd->reader_exit_sem) {
        vSemaphoreDelete(sd->reader_exit_sem);
    }

    free(sd);
    ESP_LOGI(TAG, "SDIO transport destroyed");
    return ESP_OK;
}

static esp_err_t sdio_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    sdio_impl_t *sd = (sdio_impl_t *)transport;
    return sdio_send_data(sd, data, len);
}

esp_err_t esp_service_mcp_trans_sdio_create(const esp_service_mcp_sdio_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    sdio_impl_t *sd = calloc(1, sizeof(sdio_impl_t));
    if (!sd) {
        return ESP_ERR_NO_MEM;
    }

    esp_service_mcp_trans_init(&sd->base, ESP_SERVICE_MCP_TRANS_SDIO, TAG, &sdio_ops);

    if (config) {
        sd->config = *config;
    } else {
        esp_service_mcp_sdio_config_t default_cfg = ESP_SERVICE_MCP_SDIO_CONFIG_DEFAULT();
        sd->config = default_cfg;
    }

    sd->running = false;
    sd->recv_buffers = NULL;

    sd->write_mutex = xSemaphoreCreateMutex();
    if (!sd->write_mutex) {
        free(sd);
        return ESP_ERR_NO_MEM;
    }
    sd->reader_exit_sem = xSemaphoreCreateBinary();
    if (!sd->reader_exit_sem) {
        vSemaphoreDelete(sd->write_mutex);
        free(sd);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "SDIO transport created (buf_size=%d, buf_count=%d)",
             (int)sd->config.recv_buffer_size, sd->config.recv_buffer_count);

    *out_transport = &sd->base;
    return ESP_OK;
}

#else  /* !SOC_SDIO_SLAVE_SUPPORTED */

esp_err_t esp_service_mcp_trans_sdio_create(const esp_service_mcp_sdio_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    (void)config;
    (void)out_transport;
    ESP_LOGE(TAG, "SDIO slave is not supported on this chip");
    return ESP_ERR_NOT_SUPPORTED;
}

#endif  /* SOC_SDIO_SLAVE_SUPPORTED */
