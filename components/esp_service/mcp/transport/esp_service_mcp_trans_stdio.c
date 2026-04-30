/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_service_mcp_trans_stdio.h"

static const char *TAG = "MCP_STDIO";

/**
 * @brief  Concrete STDIO transport structure
 */
typedef struct {
    esp_service_mcp_trans_t         base;  /*!< MUST be first field */
    esp_service_mcp_stdio_config_t  config;
    TaskHandle_t                    reader_task;
    SemaphoreHandle_t               write_mutex;      /*!< Protect stdout writes */
    SemaphoreHandle_t               reader_exit_sem;  /*!< Signalled by reader task before vTaskDelete */
    bool                            running;
} stdio_impl_t;

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t stdio_start(esp_service_mcp_trans_t *transport);
static esp_err_t stdio_stop(esp_service_mcp_trans_t *transport);
static esp_err_t stdio_destroy(esp_service_mcp_trans_t *transport);
static esp_err_t stdio_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

static const esp_service_mcp_trans_ops_t stdio_ops = {
    .start     = stdio_start,
    .stop      = stdio_stop,
    .destroy   = stdio_destroy,
    .broadcast = stdio_broadcast,
};

/* ─── I/O helpers ─── */

static esp_err_t stdio_write_line(stdio_impl_t *st, const char *line)
{
    if (xSemaphoreTake(st->write_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire write mutex");
        return ESP_ERR_TIMEOUT;
    }

    fputs(line, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    xSemaphoreGive(st->write_mutex);
    return ESP_OK;
}

static int stdio_wait_readable(int fd, uint32_t timeout_ms)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    return select(fd + 1, &rfds, NULL, NULL, &tv);
}

static void stdio_reader_task(void *arg)
{
    stdio_impl_t *st = (stdio_impl_t *)arg;

    char *line_buf = malloc(st->config.max_request_size);
    if (!line_buf) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        st->running = false;
        st->reader_task = NULL;
        xSemaphoreGive(st->reader_exit_sem);
        vTaskDelete(NULL);
        return;
    }

    const int fd = fileno(stdin);
    int saved_flags = fcntl(fd, F_GETFL, 0);
    if (saved_flags >= 0) {
        fcntl(fd, F_SETFL, saved_flags | O_NONBLOCK);
    }

    ESP_LOGI(TAG, "STDIO reader task started");

    size_t pos = 0;
    while (st->running) {
        int sel = stdio_wait_readable(fd, 100);
        if (sel <= 0) {
            continue;
        }
        char ch;
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch != '\n' && pos + 1 < st->config.max_request_size) {
            line_buf[pos++] = ch;
            continue;
        }

        line_buf[pos] = '\0';
        if (pos == 0) {
            continue;
        }

        ESP_LOGD(TAG, "Received line (%u bytes): %s", (unsigned)pos, line_buf);

        char *response_str = NULL;
        esp_err_t ret = esp_service_mcp_trans_dispatch_request(&st->base, line_buf, &response_str);
        pos = 0;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Request dispatch failed: %s", esp_err_to_name(ret));
            continue;
        }
        if (response_str) {
            stdio_write_line(st, response_str);
            ESP_LOGD(TAG, "Sent response: %s", response_str);
            free(response_str);
        }
    }

    if (saved_flags >= 0) {
        fcntl(fd, F_SETFL, saved_flags);
    }
    free(line_buf);
    ESP_LOGI(TAG, "STDIO reader task exiting");
    xSemaphoreGive(st->reader_exit_sem);
    vTaskDelete(NULL);
}

static esp_err_t stdio_start(esp_service_mcp_trans_t *transport)
{
    stdio_impl_t *st = (stdio_impl_t *)transport;

    if (st->running) {
        ESP_LOGW(TAG, "STDIO transport already running");
        return ESP_OK;
    }

    st->running = true;

    BaseType_t ret;
    if (st->config.task_core >= 0) {
        ret = xTaskCreatePinnedToCore(stdio_reader_task, "mcp_stdio",
                                      st->config.stack_size, st,
                                      st->config.task_priority,
                                      &st->reader_task,
                                      st->config.task_core);
    } else {
        ret = xTaskCreate(stdio_reader_task, "mcp_stdio",
                          st->config.stack_size, st,
                          st->config.task_priority,
                          &st->reader_task);
    }

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reader task");
        st->running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "STDIO transport started");
    ESP_LOGI(TAG, "Send JSON-RPC requests via stdin (one per line)");
    return ESP_OK;
}

static esp_err_t stdio_stop(esp_service_mcp_trans_t *transport)
{
    stdio_impl_t *st = (stdio_impl_t *)transport;

    if (!st->running) {
        return ESP_OK;
    }

    st->running = false;

    if (st->reader_task) {
        if (xSemaphoreTake(st->reader_exit_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGW(TAG, "Reader task did not exit within 2s; forcing delete");
            vTaskDelete(st->reader_task);
        }
        st->reader_task = NULL;
    }

    ESP_LOGI(TAG, "STDIO transport stopped");
    return ESP_OK;
}

static esp_err_t stdio_destroy(esp_service_mcp_trans_t *transport)
{
    stdio_impl_t *st = (stdio_impl_t *)transport;

    if (st->running) {
        stdio_stop(transport);
    }

    if (st->write_mutex) {
        vSemaphoreDelete(st->write_mutex);
    }
    if (st->reader_exit_sem) {
        vSemaphoreDelete(st->reader_exit_sem);
    }

    free(st);
    ESP_LOGI(TAG, "STDIO transport destroyed");
    return ESP_OK;
}

static esp_err_t stdio_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    stdio_impl_t *st = (stdio_impl_t *)transport;
    (void)len;  /* We use null-terminated string write */
    return stdio_write_line(st, data);
}

esp_err_t esp_service_mcp_trans_stdio_create(const esp_service_mcp_stdio_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    stdio_impl_t *st = calloc(1, sizeof(stdio_impl_t));
    if (!st) {
        return ESP_ERR_NO_MEM;
    }

    esp_service_mcp_trans_init(&st->base, ESP_SERVICE_MCP_TRANS_STDIO, TAG, &stdio_ops);

    if (config) {
        st->config = *config;
    } else {
        esp_service_mcp_stdio_config_t default_cfg = ESP_SERVICE_MCP_STDIO_CONFIG_DEFAULT();
        st->config = default_cfg;
    }

    st->running = false;

    st->write_mutex = xSemaphoreCreateMutex();
    if (!st->write_mutex) {
        free(st);
        return ESP_ERR_NO_MEM;
    }
    st->reader_exit_sem = xSemaphoreCreateBinary();
    if (!st->reader_exit_sem) {
        vSemaphoreDelete(st->write_mutex);
        free(st);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "STDIO transport created (max_request_size=%d)",
             (int)st->config.max_request_size);

    *out_transport = &st->base;
    return ESP_OK;
}
