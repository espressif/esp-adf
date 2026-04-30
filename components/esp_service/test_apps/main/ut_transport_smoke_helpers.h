/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_transport_smoke_helpers.h
 * @brief Shared Unity checks for on-device MCP transport smoke tests.
 */

#pragma once

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_service_mcp_server.h"

/**
 * @brief Poll until *got_ip is true or timeout elapses; fails the test if no IP.
 */
static inline void ut_transport_wait_sta_ip(volatile bool *got_ip, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while (!*got_ip && (xTaskGetTickCount() - start) < timeout_ticks) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    TEST_ASSERT_TRUE_MESSAGE(*got_ip, "STA did not get IP within timeout");
}

/**
 * @brief Call tools/list on the MCP server and assert a plausible JSON-RPC success payload.
 */
static inline void ut_transport_assert_mcp_tools_list(esp_service_mcp_server_t *mcp)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
    esp_service_mcp_response_t resp;
    memset(&resp, 0, sizeof(resp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_handle_request(mcp, req, &resp));
    TEST_ASSERT_EQUAL(ESP_OK, resp.error);
    TEST_ASSERT_NOT_NULL(resp.response);
    TEST_ASSERT_NOT_NULL(strstr(resp.response, "\"result\""));
    TEST_ASSERT_NOT_NULL(strstr(resp.response, "player_service"));
    esp_service_mcp_response_free(&resp);
}
