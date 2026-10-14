/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "unity.h"
#include "unity_test_utils.h"

#include "esp_tongyi_transport.h"

static void init_rx(esp_tongyi_rx_assembler_t *rx)
{
    memset(rx, 0, sizeof(*rx));
    rx->max_json_bytes = 64;
    rx->max_binary_bytes = 64;
}

TEST_CASE("assembler rejects negative fragment fields", "[esp_tongyi]")
{
    esp_tongyi_rx_assembler_t rx;
    init_rx(&rx);
    bool complete = false;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, -1, 0, "x", 1, &complete));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 1, -1, "x", 1, &complete));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 1, 0, "x", -3, &complete));
    esp_tongyi_rx_deinit(&rx);
}

TEST_CASE("assembler rejects out of order and length change", "[esp_tongyi]")
{
    esp_tongyi_rx_assembler_t rx;
    init_rx(&rx);
    bool complete = false;
    TEST_ESP_OK(esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 4, 0, "ab", 2, &complete));
    TEST_ASSERT_FALSE(complete);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 4, 3, "c", 1, &complete));
    esp_tongyi_rx_deinit(&rx);

    init_rx(&rx);
    TEST_ESP_OK(esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 4, 0, "ab", 2, &complete));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 8, 2, "cd", 2, &complete));
    esp_tongyi_rx_deinit(&rx);
}

TEST_CASE("assembler rejects oversized and overflowing fragments", "[esp_tongyi]")
{
    esp_tongyi_rx_assembler_t rx;
    init_rx(&rx);
    bool complete = false;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 128, 0, "x", 1, &complete));
    esp_tongyi_rx_deinit(&rx);

    init_rx(&rx);
    TEST_ESP_OK(esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_BIN, 4, 0, "ab", 2, &complete));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_BIN, 4, 2, "cdef", 4, &complete));
    esp_tongyi_rx_deinit(&rx);
}

TEST_CASE("assembler reassembles ordered fragments", "[esp_tongyi]")
{
    esp_tongyi_rx_assembler_t rx;
    init_rx(&rx);
    bool complete = false;
    TEST_ESP_OK(esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 4, 0, "he", 2, &complete));
    TEST_ASSERT_FALSE(complete);
    TEST_ESP_OK(esp_tongyi_rx_feed(&rx, ESP_TONGYI_WS_OPCODE_TEXT, 4, 2, "lo", 2, &complete));
    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_EQUAL_STRING("helo", (const char *)rx.buf);
    TEST_ASSERT_EQUAL(4, rx.received);
    esp_tongyi_rx_deinit(&rx);
}

TEST_CASE("send length helper rejects partial writes", "[esp_tongyi]")
{
    TEST_ASSERT_TRUE(esp_tongyi_transport_len_ok(16));
    TEST_ASSERT_FALSE(esp_tongyi_transport_len_ok(0));
    TEST_ESP_OK(esp_tongyi_transport_check_sent(4, 4));
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_tongyi_transport_check_sent(8, 3));
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_tongyi_transport_check_sent(8, -1));
}
