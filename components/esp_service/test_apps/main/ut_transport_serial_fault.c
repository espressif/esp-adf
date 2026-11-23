/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#include "unity.h"

static void *reader_malloc(size_t size);
static void *dma_malloc(size_t size, uint32_t caps);
static void serial_free(void *buffer);
static ssize_t serial_read(int fd, void *buffer, size_t len);
static int serial_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
static int serial_fcntl(int fd, int command, ...);

#define malloc               reader_malloc
#define free                 serial_free
#define TAG                  TAG_UART
#define uart_param_config    scripted_uart_param_config
#define uart_set_pin         scripted_uart_set_pin
#define uart_driver_install  scripted_uart_driver_install
#define uart_driver_delete   scripted_uart_driver_delete
#define uart_write_bytes     scripted_uart_write_bytes
#define uart_read_bytes      scripted_uart_read_bytes
#include "../../mcp/transport/esp_service_mcp_trans_uart.c"
#undef TAG
#define TAG     TAG_STDIO
#define read    serial_read
#define select  serial_select
#define fcntl   serial_fcntl
#include "../../mcp/transport/esp_service_mcp_trans_stdio.c"
#undef read
#undef select
#undef fcntl
#undef TAG
#undef SOC_SDIO_SLAVE_SUPPORTED
#define SOC_SDIO_SLAVE_SUPPORTED  1
#define heap_caps_malloc          dma_malloc
#define TAG                       TAG_SDIO
#include "../../mcp/transport/esp_service_mcp_trans_sdio.c"
#undef TAG
#undef heap_caps_malloc
#undef malloc
#undef free

typedef struct {
    const char *data;
    size_t      len;
    bool        last;
} fragment_t;

/* Exercise production reader/stop code under FreeRTOS; only device I/O is scripted. */
static bool fail_reader_malloc;
static int reader_frees;
static void *reader_buffer;
static int response_frees;
static void *response_buffer;
static void *dma_buffers[8];
static int dma_allocations;
static int dma_frees;
static void *queued_dma;

static const char *uart_input;
static size_t uart_pos;
static int uart_deletes;
static esp_err_t uart_delete_result;
static const char *stdio_input;
static size_t stdio_pos;
static int stdin_flags;
static int stdin_flag_writes;

static fragment_t fragments[8];
static size_t fragment_count;
static size_t fragment_pos;
static int sdio_deinits;
static int sdio_reloads;
static esp_err_t queue_result;
static bool send_finished;

static SemaphoreHandle_t entered;
static SemaphoreHandle_t release_handler;
static atomic_int calls;
static bool block_handler;
static bool destroy_in_handler;
static esp_err_t callback_destroy_result;
static esp_err_t handler_result;
static char last_request[32];

static void *reader_malloc(size_t size)
{
    if (fail_reader_malloc) {
        fail_reader_malloc = false;
        return NULL;
    }
    reader_buffer = malloc(size);
    return reader_buffer;
}

static void *dma_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    void *buffer = malloc(size);
    TEST_ASSERT_NOT_NULL(buffer);
    for (size_t i = 0; i < 8; i++) {
        if (!dma_buffers[i]) {
            dma_buffers[i] = buffer;
            dma_allocations++;
            return buffer;
        }
    }
    TEST_FAIL_MESSAGE("DMA ownership tracking overflow");
    return NULL;
}

static void serial_free(void *buffer)
{
    if (buffer && buffer == reader_buffer) {
        reader_frees++;
        reader_buffer = NULL;
    }
    if (buffer && buffer == response_buffer) {
        response_frees++;
        response_buffer = NULL;
    }
    for (size_t i = 0; buffer && i < 8; i++) {
        if (buffer == dma_buffers[i]) {
            TEST_ASSERT_NOT_EQUAL((uintptr_t)queued_dma, (uintptr_t)buffer);
            dma_buffers[i] = NULL;
            dma_frees++;
            break;
        }
    }
    free(buffer);
}

static ssize_t serial_read(int fd, void *buffer, size_t len)
{
    (void)fd;
    (void)len;
    if (stdio_input && stdio_input[stdio_pos]) {
        *(char *)buffer = stdio_input[stdio_pos++];
        return 1;
    }
    errno = EAGAIN;
    return -1;
}

static int serial_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)readfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;
    vTaskDelay(1);
    return stdio_input && stdio_input[stdio_pos] ? 1 : 0;
}

static int serial_fcntl(int fd, int command, ...)
{
    (void)fd;
    if (command == F_GETFL) {
        return stdin_flags;
    }
    va_list args;
    va_start(args, command);
    stdin_flags = va_arg(args, int);
    va_end(args);
    stdin_flag_writes++;
    return 0;
}

static esp_err_t request_handler(const char *request, char **response, void *ctx)
{
    if (destroy_in_handler) {
        callback_destroy_result = esp_service_mcp_trans_destroy(ctx);
    }
    snprintf(last_request, sizeof(last_request), "%s", request);
    calls++;
    xSemaphoreGive(entered);
    if (block_handler) {
        xSemaphoreTake(release_handler, portMAX_DELAY);
    }
    *response = strdup("{}");
    response_buffer = *response;
    return handler_result;
}

static void reset_script(void)
{
    uart_input = NULL;
    uart_pos = 0;
    uart_deletes = 0;
    uart_delete_result = ESP_OK;
    stdio_input = NULL;
    stdio_pos = 0;
    stdin_flags = 0;
    stdin_flag_writes = 0;
    fragment_count = 0;
    fragment_pos = 0;
    sdio_deinits = 0;
    sdio_reloads = 0;
    queue_result = ESP_OK;
    send_finished = false;
    fail_reader_malloc = false;
    reader_frees = 0;
    response_frees = 0;
    dma_allocations = 0;
    dma_frees = 0;
    calls = 0;
    block_handler = false;
    destroy_in_handler = false;
    handler_result = ESP_OK;
    entered = xSemaphoreCreateBinary();
    release_handler = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(entered);
    TEST_ASSERT_NOT_NULL(release_handler);
}

static void finish_script(void)
{
    TEST_ASSERT_NULL(reader_buffer);
    TEST_ASSERT_NULL(response_buffer);
    TEST_ASSERT_NULL(queued_dma);
    TEST_ASSERT_EQUAL(dma_allocations, dma_frees);
    vSemaphoreDelete(entered);
    vSemaphoreDelete(release_handler);
}

static esp_service_mcp_trans_t *create_serial(int type)
{
    esp_service_mcp_trans_t *transport = NULL;
    if (type == 0) {
        esp_service_mcp_uart_config_t config = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
        config.max_request_size = 8;
        TEST_ESP_OK(esp_service_mcp_trans_uart_create(&config, &transport));
    } else if (type == 1) {
        esp_service_mcp_stdio_config_t config = ESP_SERVICE_MCP_STDIO_CONFIG_DEFAULT();
        config.max_request_size = 8;
        TEST_ESP_OK(esp_service_mcp_trans_stdio_create(&config, &transport));
    } else {
        esp_service_mcp_sdio_config_t config = ESP_SERVICE_MCP_SDIO_CONFIG_DEFAULT();
        config.recv_buffer_size = 8;
        config.recv_buffer_count = 2;
        TEST_ESP_OK(esp_service_mcp_trans_sdio_create(&config, &transport));
    }
    TEST_ESP_OK(esp_service_mcp_trans_set_request_handler(transport, request_handler, transport));
    return transport;
}

TEST_CASE("serial destroy timeout retains reader and resources for one successful retry", "[transport]")
{
    for (int type = 0; type < 3; type++) {
        reset_script();
        uart_input = "{}\n";
        stdio_input = "{}\n";
        fragments[0] = (fragment_t) {"{}", 2, true};
        fragment_count = 1;
        block_handler = true;
        /* Even an error-producing handler must release its response. */
        handler_result = ESP_FAIL;
        esp_service_mcp_trans_t *transport = create_serial(type);
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
        TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_mcp_trans_destroy(transport));
        TEST_ASSERT_EQUAL(0, reader_frees);
        TEST_ASSERT_EQUAL(0, uart_deletes);
        TEST_ASSERT_EQUAL(0, sdio_deinits);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_mcp_trans_start(transport));
        xSemaphoreGive(release_handler);
        TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
        TEST_ASSERT_EQUAL(1, reader_frees);
        TEST_ASSERT_EQUAL(1, response_frees);
        TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
        TEST_ASSERT_EQUAL(type == 0, uart_deletes);
        TEST_ASSERT_EQUAL(type == 2, sdio_deinits);
        if (type == 1) {
            TEST_ASSERT_EQUAL(0, stdin_flags);
            TEST_ASSERT_EQUAL(2, stdin_flag_writes);
        }
        TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
        TEST_ASSERT_EQUAL(type == 0, uart_deletes);
        TEST_ASSERT_EQUAL(type == 2, sdio_deinits);
        finish_script();
    }
}

TEST_CASE("serial readers reject entire oversized message and recover at next boundary", "[transport]")
{
    for (int type = 0; type < 3; type++) {
        reset_script();
        uart_input = "12345678{}\n{}\n";
        stdio_input = "12345678{}\n{}\n";
        fragments[0] = (fragment_t) {"12345678", 8, false};
        fragments[1] = (fragment_t) {"{}", 2, true};
        fragments[2] = (fragment_t) {"{", 1, false};
        fragments[3] = (fragment_t) {"}\0", 2, true};
        fragment_count = 4;
        esp_service_mcp_trans_t *transport = create_serial(type);
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
        TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
        TEST_ASSERT_EQUAL(1, calls);
        TEST_ASSERT_EQUAL_STRING("{}", last_request);
        TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
        finish_script();
    }
}

TEST_CASE("line collector accepts exact capacity and rejects NUL and oversized suffixes", "[transport]")
{
    char buffer[8];
    size_t pos = 0;
    bool discard = false;
    const char input[] = "1234567\n12345678{}\n{\0}\n{}\r\n";
    int accepted = 0;
    int rejected = 0;
    for (size_t i = 0; i < sizeof(input) - 1; i++) {
        int result = mcp_line_feed(buffer, sizeof(buffer), &pos, &discard, input[i]);
        if (result > 0) {
            TEST_ASSERT_EQUAL_STRING(accepted == 0 ? "1234567" : "{}", buffer);
            accepted++;
        } else if (result < 0) {
            rejected++;
        }
    }
    TEST_ASSERT_EQUAL(2, accepted);
    TEST_ASSERT_EQUAL(2, rejected);
}

TEST_CASE("SDIO timeout keeps one DMA owner and stop cancels before freeing", "[transport]")
{
    reset_script();
    esp_service_mcp_trans_t *transport = create_serial(2);
    TEST_ESP_OK(esp_service_mcp_trans_start(transport));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, sdio_send_data((sdio_impl_t *)transport, "{}", 3));
    int allocated = dma_allocations;
    TEST_ASSERT_NOT_NULL(((sdio_impl_t *)transport)->pending_send);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, sdio_send_data((sdio_impl_t *)transport, "{}", 3));
    TEST_ASSERT_EQUAL(allocated, dma_allocations);
    TEST_ASSERT_EQUAL(0, dma_frees);
    send_finished = true;
    TEST_ESP_OK(sdio_send_data((sdio_impl_t *)transport, "{}", 3));
    TEST_ASSERT_NULL(((sdio_impl_t *)transport)->pending_send);
    TEST_ASSERT_EQUAL(2, dma_frees);
    send_finished = false;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, sdio_send_data((sdio_impl_t *)transport, "{}", 3));
    TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
    TEST_ASSERT_EQUAL(1, sdio_deinits);
    TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
    TEST_ASSERT_EQUAL(1, sdio_deinits);
    finish_script();
}

TEST_CASE("SDIO - packet boundaries: exact size preserved and embedded NUL rejected", "[transport]")
{
    for (int invalid = 0; invalid < 2; invalid++) {
        reset_script();
        if (invalid) {
            fragments[0] = (fragment_t) {"{\0", 2, false};
            fragments[1] = (fragment_t) {"}", 1, true};
            fragments[2] = (fragment_t) {"{}", 2, true};
            fragment_count = 3;
        } else {
            fragments[0] = (fragment_t) {"1234", 4, false};
            fragments[1] = (fragment_t) {"5678", 4, true};
            fragment_count = 2;
        }
        esp_service_mcp_trans_t *transport = create_serial(2);
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
        TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
        TEST_ASSERT_EQUAL(1, calls);
        TEST_ASSERT_EQUAL_STRING(invalid ? "{}" : "12345678", last_request);
        TEST_ASSERT_EQUAL(2 + fragment_count, sdio_reloads);
        TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
        finish_script();
    }
}

TEST_CASE("SDIO - queue failure and busy writer: rollback or retain DMA exactly once", "[transport]")
{
    reset_script();
    esp_service_mcp_trans_t *transport = create_serial(2);
    TEST_ESP_OK(esp_service_mcp_trans_start(transport));
    sdio_impl_t *sd = (sdio_impl_t *)transport;
    queue_result = ESP_FAIL;
    TEST_ASSERT_EQUAL(ESP_FAIL, sdio_send_data(sd, "{}", 3));
    TEST_ASSERT_NULL(sd->pending_send);
    TEST_ASSERT_EQUAL(1, dma_frees);
    queue_result = ESP_OK;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, sdio_send_data(sd, "{}", 3));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sd->write_mutex, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_mcp_trans_destroy(transport));
    TEST_ASSERT_EQUAL(0, sdio_deinits);
    TEST_ASSERT_NOT_NULL(sd->pending_send);
    TEST_ASSERT_EQUAL(1, dma_frees);
    xSemaphoreGive(sd->write_mutex);
    TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
    TEST_ASSERT_EQUAL(1, sdio_deinits);
    finish_script();
}

TEST_CASE("UART driver cleanup failure preserves installed resource for retry", "[transport]")
{
    reset_script();
    esp_service_mcp_trans_t *transport = create_serial(0);
    TEST_ESP_OK(esp_service_mcp_trans_start(transport));
    uart_delete_result = ESP_FAIL;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_service_mcp_trans_destroy(transport));
    TEST_ASSERT_TRUE(((uart_impl_t *)transport)->uart_installed);
    TEST_ASSERT_NULL(((uart_impl_t *)transport)->reader_task);
    uart_delete_result = ESP_OK;
    TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
    TEST_ASSERT_EQUAL(2, uart_deletes);
    TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
    TEST_ASSERT_EQUAL(2, uart_deletes);
    finish_script();
}

TEST_CASE("reader allocation failure requires stop before restart and drains exit token", "[transport]")
{
    for (int type = 0; type < 3; type++) {
        reset_script();
        esp_service_mcp_trans_t *transport = create_serial(type);
        fail_reader_malloc = true;
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        SemaphoreHandle_t exited = type == 0 ? ((uart_impl_t *)transport)->reader_exit_sem : type == 1 ? ((stdio_impl_t *)transport)->reader_exit_sem
                                                                                                       : ((sdio_impl_t *)transport)->reader_exit_sem;
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(exited, pdMS_TO_TICKS(1000)));
        xSemaphoreGive(exited);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_mcp_trans_start(transport));
        TEST_ESP_OK(esp_service_mcp_trans_stop(transport));
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
        TEST_ASSERT_EQUAL(1, reader_frees);
        finish_script();
    }
}

TEST_CASE("serial - callback destroy: rejected before any resource release", "[transport]")
{
    for (int type = 0; type < 3; type++) {
        reset_script();
        uart_input = "{}\n";
        stdio_input = "{}\n";
        fragments[0] = (fragment_t) {"{}", 2, true};
        fragment_count = 1;
        destroy_in_handler = true;
        esp_service_mcp_trans_t *transport = create_serial(type);
        TEST_ESP_OK(esp_service_mcp_trans_start(transport));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, callback_destroy_result);
        TEST_ASSERT_EQUAL(0, uart_deletes);
        TEST_ASSERT_EQUAL(0, sdio_deinits);
        TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
        finish_script();
    }
}

TEST_CASE("SDIO - invalid receive configuration: constructor rejects without publishing handle", "[transport]")
{
    const size_t sizes[] = {0, 1, SDIO_SLAVE_RECV_MAX_BUFFER + 1, 4096, SIZE_MAX, 8, 8};
    const int counts[] = {2, 2, 2, 2, 2, 0, -1};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        esp_service_mcp_sdio_config_t config = ESP_SERVICE_MCP_SDIO_CONFIG_DEFAULT();
        config.recv_buffer_size = sizes[i];
        config.recv_buffer_count = counts[i];
        esp_service_mcp_trans_t *transport = NULL;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_mcp_trans_sdio_create(&config, &transport));
        TEST_ASSERT_NULL(transport);
    }

    esp_service_mcp_sdio_config_t max_config = ESP_SERVICE_MCP_SDIO_CONFIG_DEFAULT();
    max_config.recv_buffer_size = SDIO_SLAVE_RECV_MAX_BUFFER;
    esp_service_mcp_trans_t *transport = NULL;
    TEST_ESP_OK(esp_service_mcp_trans_sdio_create(&max_config, &transport));
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ESP_OK(esp_service_mcp_trans_destroy(transport));
}
void transport_serial_fault_force_link(void)
{
}

esp_err_t scripted_uart_param_config(uart_port_t port, const uart_config_t *config)
{
    (void)port;
    (void)config;
    return ESP_OK;
}

esp_err_t scripted_uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts)
{
    (void)port;
    (void)tx;
    (void)rx;
    (void)rts;
    (void)cts;
    return ESP_OK;
}

esp_err_t scripted_uart_driver_install(uart_port_t port, int rx, int tx, int size, void *queue, int flags)
{
    (void)port;
    (void)rx;
    (void)tx;
    (void)size;
    (void)queue;
    (void)flags;
    return ESP_OK;
}

esp_err_t scripted_uart_driver_delete(uart_port_t port)
{
    (void)port;
    uart_deletes++;
    return uart_delete_result;
}

int scripted_uart_write_bytes(uart_port_t port, const void *data, size_t size)
{
    (void)port;
    (void)data;
    return (int)size;
}

int scripted_uart_read_bytes(uart_port_t port, void *buffer, uint32_t length, uint32_t wait)
{
    (void)port;
    (void)length;
    (void)wait;
    if (uart_input && uart_input[uart_pos]) {
        *(char *)buffer = uart_input[uart_pos++];
        return 1;
    }
    vTaskDelay(1);
    return 0;
}

esp_err_t sdio_slave_initialize(sdio_slave_config_t *config)
{
    (void)config;
    return ESP_OK;
}

void sdio_slave_deinit(void)
{
    sdio_deinits++;
    queued_dma = NULL;
}

esp_err_t sdio_slave_start(void)
{
    return ESP_OK;
}

void sdio_slave_stop(void)
{
}

sdio_slave_buf_handle_t sdio_slave_recv_register_buf(uint8_t *buffer)
{
    return buffer;
}

esp_err_t sdio_slave_recv_load_buf(sdio_slave_buf_handle_t handle)
{
    (void)handle;
    sdio_reloads++;
    return ESP_OK;
}

esp_err_t sdio_slave_recv_packet(sdio_slave_buf_handle_t *handle, uint32_t wait)
{
    (void)wait;
    if (fragment_pos < fragment_count) {
        fragment_t *fragment = &fragments[fragment_pos++];
        *handle = fragment;
        return fragment->last ? ESP_OK : ESP_ERR_NOT_FINISHED;
    }
    vTaskDelay(1);
    return ESP_ERR_TIMEOUT;
}

uint8_t *sdio_slave_recv_get_buf(sdio_slave_buf_handle_t handle, size_t *len)
{
    fragment_t *fragment = handle;
    *len = fragment->len;
    return (uint8_t *)fragment->data;
}

esp_err_t sdio_slave_send_queue(uint8_t *buffer, size_t len, void *arg, uint32_t wait)
{
    (void)len;
    (void)arg;
    (void)wait;
    TEST_ASSERT_NULL(queued_dma);
    if (queue_result == ESP_OK) {
        queued_dma = buffer;
    }
    return queue_result;
}

esp_err_t sdio_slave_send_get_finished(void **arg, uint32_t wait)
{
    (void)wait;
    if (!send_finished) {
        return ESP_ERR_TIMEOUT;
    }
    *arg = queued_dma;
    queued_dma = NULL;
    return ESP_OK;
}
