/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_service_mcp_trans_uart.h"
#include "mcp_line_reader.h"

/**
 * @brief  Concrete UART transport structure
 */
typedef struct {
    esp_service_mcp_trans_t        base;  /*!< MUST be first field */
    esp_service_mcp_uart_config_t  config;
    TaskHandle_t                   reader_task;
    SemaphoreHandle_t              write_mutex;      /*!< Protect UART writes */
    SemaphoreHandle_t              reader_exit_sem;  /*!< Signalled by reader task before vTaskDelete */
    atomic_bool                    running;
    bool                           uart_installed;
} uart_impl_t;

static const char *TAG = "MCP_UART";

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t uart_start(esp_service_mcp_trans_t *transport);
static esp_err_t uart_stop(esp_service_mcp_trans_t *transport);
static esp_err_t uart_destroy(esp_service_mcp_trans_t *transport);
static esp_err_t uart_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

static const esp_service_mcp_trans_ops_t uart_ops = {
    .start     = uart_start,
    .stop      = uart_stop,
    .destroy   = uart_destroy,
    .broadcast = uart_broadcast,
};

/* ─── I/O helpers ─── */

static esp_err_t uart_write_line(uart_impl_t *ut, const char *line)
{
    if (xSemaphoreTake(ut->write_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire write mutex");
        return ESP_ERR_TIMEOUT;
    }
    if (!ut->running) {
        xSemaphoreGive(ut->write_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    size_t len = strlen(line);
    int written = uart_write_bytes(ut->config.uart_port, line, len);
    if (written < 0) {
        xSemaphoreGive(ut->write_mutex);
        return ESP_FAIL;
    }
    uart_write_bytes(ut->config.uart_port, "\n", 1);

    xSemaphoreGive(ut->write_mutex);
    return ESP_OK;
}

static int uart_read_line(uart_impl_t *ut, char *buffer, size_t buffer_size)
{
    size_t pos = 0;
    bool discard = false;
    while (ut->running) {
        uint8_t ch;
        int len = uart_read_bytes(ut->config.uart_port, &ch, 1, pdMS_TO_TICKS(100));

        if (len <= 0) {
            if (!ut->running) {
                return -1;
            }
            continue;
        }

        int result = mcp_line_feed(buffer, buffer_size, &pos, &discard, (char)ch);
        if (result != 0) {
            if (result < 0) {
                ESP_LOGW(TAG, "Receive: oversized or invalid line discarded");
                return -1;
            }
            return (int)strlen(buffer);
        }
    }

    return -1;
}

static void uart_reader_task(void *arg)
{
    uart_impl_t *ut = (uart_impl_t *)arg;

    char *line_buf = malloc(ut->config.max_request_size);
    if (!line_buf) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        ut->running = false;
        xSemaphoreGive(ut->reader_exit_sem);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART reader task started (port=%d)", ut->config.uart_port);

    while (ut->running) {
        int len = uart_read_line(ut, line_buf, ut->config.max_request_size);
        if (len < 0) {
            continue;
        }
        if (len == 0) {
            continue;
        }

        ESP_LOGD(TAG, "Received line: %d bytes", len);

        /* Dispatch through the abstract transport layer */
        char *response_str = NULL;
        esp_err_t ret = esp_service_mcp_trans_dispatch_request(&ut->base, line_buf, &response_str);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Request dispatch failed: %s", esp_err_to_name(ret));
        }

        if (response_str) {
            if (ret == ESP_OK && ut->running) {
                uart_write_line(ut, response_str);
            }
            ESP_LOGD(TAG, "Sent response: %u bytes", (unsigned)strlen(response_str));
            free(response_str);
        }
    }

    free(line_buf);
    ESP_LOGI(TAG, "UART reader task exiting");
    xSemaphoreGive(ut->reader_exit_sem);
    vTaskDelete(NULL);
}

static esp_err_t uart_start(esp_service_mcp_trans_t *transport)
{
    uart_impl_t *ut = (uart_impl_t *)transport;

    if (ut->running) {
        ESP_LOGW(TAG, "UART transport already running");
        return ESP_OK;
    }
    if (ut->reader_task || ut->uart_installed) {
        ESP_LOGW(TAG, "Start: previous stop must complete first");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(ut->reader_exit_sem, 0);

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate = ut->config.baud_rate,
        .data_bits = ut->config.data_bits,
        .parity = ut->config.parity,
        .stop_bits = ut->config.stop_bits,
        .flow_ctrl = ut->config.flow_ctrl,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(ut->config.uart_port, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(ut->config.uart_port,
                       ut->config.tx_pin, ut->config.rx_pin,
                       ut->config.rts_pin, ut->config.cts_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(ut->config.uart_port,
                              ut->config.rx_buffer_size,
                              ut->config.tx_buffer_size,
                              0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    ut->uart_installed = true;

    ut->running = true;

    /* Create reader task */
    BaseType_t xret;
    if (ut->config.task_core >= 0) {
        xret = xTaskCreatePinnedToCore(uart_reader_task, "mcp_uart",
                                       ut->config.stack_size, ut,
                                       ut->config.task_priority,
                                       &ut->reader_task,
                                       ut->config.task_core);
    } else {
        xret = xTaskCreate(uart_reader_task, "mcp_uart",
                           ut->config.stack_size, ut,
                           ut->config.task_priority,
                           &ut->reader_task);
    }

    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reader task");
        ut->running = false;
        uart_driver_delete(ut->config.uart_port);
        ut->uart_installed = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UART transport started (port=%d, baud=%d, tx=%d, rx=%d)",
             ut->config.uart_port, ut->config.baud_rate,
             ut->config.tx_pin, ut->config.rx_pin);
    return ESP_OK;
}

static esp_err_t uart_stop(esp_service_mcp_trans_t *transport)
{
    uart_impl_t *ut = (uart_impl_t *)transport;

    if (ut->reader_task && ut->reader_task == xTaskGetCurrentTaskHandle()) {
        ESP_LOGW(TAG, "Stop: reader cannot stop itself");
        return ESP_ERR_INVALID_STATE;
    }

    ut->running = false;

    if (ut->reader_task) {
        if (xSemaphoreTake(ut->reader_exit_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
            ESP_LOGW(TAG, "Stop: reader has not exited; retry after the handler returns");
            return ESP_ERR_TIMEOUT;
        }
        ut->reader_task = NULL;
    }

    if (ut->uart_installed) {
        if (xSemaphoreTake(ut->write_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "Stop: writer has not exited");
            return ESP_ERR_TIMEOUT;
        }
        esp_err_t ret = uart_driver_delete(ut->config.uart_port);
        xSemaphoreGive(ut->write_mutex);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Stop: UART delete failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ut->uart_installed = false;
    }

    ESP_LOGI(TAG, "UART transport stopped");
    return ESP_OK;
}

static esp_err_t uart_destroy(esp_service_mcp_trans_t *transport)
{
    uart_impl_t *ut = (uart_impl_t *)transport;

    esp_err_t ret = uart_stop(transport);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ut->write_mutex) {
        vSemaphoreDelete(ut->write_mutex);
    }
    if (ut->reader_exit_sem) {
        vSemaphoreDelete(ut->reader_exit_sem);
    }

    free(ut);
    ESP_LOGI(TAG, "UART transport destroyed");
    return ESP_OK;
}

static esp_err_t uart_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    uart_impl_t *ut = (uart_impl_t *)transport;
    (void)len;
    return uart_write_line(ut, data);
}

esp_err_t esp_service_mcp_trans_uart_create(const esp_service_mcp_uart_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_impl_t *ut = calloc(1, sizeof(uart_impl_t));
    if (!ut) {
        return ESP_ERR_NO_MEM;
    }

    esp_service_mcp_trans_init(&ut->base, ESP_SERVICE_MCP_TRANS_UART, TAG, &uart_ops);

    if (config) {
        ut->config = *config;
    } else {
        esp_service_mcp_uart_config_t default_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
        ut->config = default_cfg;
    }

    /* uart_read_line() needs room for one byte plus the terminator */
    if (ut->config.max_request_size < 2) {
        ESP_LOGE(TAG, "Create: max_request_size=%u is too small (minimum 2)", (unsigned)ut->config.max_request_size);
        free(ut);
        return ESP_ERR_INVALID_ARG;
    }

    atomic_init(&ut->running, false);
    ut->uart_installed = false;

    ut->write_mutex = xSemaphoreCreateMutex();
    if (!ut->write_mutex) {
        free(ut);
        return ESP_ERR_NO_MEM;
    }
    ut->reader_exit_sem = xSemaphoreCreateBinary();
    if (!ut->reader_exit_sem) {
        vSemaphoreDelete(ut->write_mutex);
        free(ut);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UART transport created (port=%d, baud=%d)",
             ut->config.uart_port, ut->config.baud_rate);

    *out_transport = &ut->base;
    return ESP_OK;
}

int esp_service_mcp_trans_uart_get_port(esp_service_mcp_trans_t *transport)
{
    if (!transport || transport->type != ESP_SERVICE_MCP_TRANS_UART) {
        return -1;
    }
    uart_impl_t *ut = (uart_impl_t *)transport;
    return (int)ut->config.uart_port;
}
