/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "unity_test_utils.h"

#include "esp_tongyi.h"
#include "esp_tongyi_utils.h"

static esp_tongyi_cfg_t valid_cfg(void)
{
    esp_tongyi_cfg_t cfg = ESP_TONGYI_CFG_DEFAULT();
    cfg.api_key = "test_key";
    cfg.workspace_id = "ws";
    cfg.app_id = "app";
    cfg.reconnect.enabled = false;
    cfg.keep_alive_time_sec = 0;
    return cfg;
}

TEST_CASE("create rejects invalid config", "[esp_tongyi]")
{
    esp_tongyi_handle_t handle = (esp_tongyi_handle_t)0x1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_create(NULL, &handle));
    TEST_ASSERT_NULL(handle);

    esp_tongyi_cfg_t cfg = valid_cfg();
    cfg.api_key = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_create(&cfg, &handle));

    cfg = valid_cfg();
    cfg.transport.uri = "http://example.com";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_create(&cfg, &handle));

    cfg = valid_cfg();
    cfg.biz_params_json = "[1,2]";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_create(&cfg, &handle));
}

TEST_CASE("create generates task id and destroy is safe", "[esp_tongyi]")
{
    esp_tongyi_cfg_t cfg = valid_cfg();
    esp_tongyi_handle_t handle = NULL;
    TEST_ESP_OK(esp_tongyi_create(&cfg, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    bool ready = false;
    TEST_ESP_OK(esp_tongyi_is_ready(handle, &ready));
    TEST_ASSERT_FALSE(ready);
    TEST_ASSERT_EQUAL(ESP_TONGYI_STATE_IDLE, esp_tongyi_get_state(handle));

    char task_id[ESP_TONGYI_UUID_BUF_LEN];
    TEST_ESP_OK(esp_tongyi_get_task_id(handle, task_id, sizeof(task_id)));
    TEST_ASSERT_EQUAL(ESP_TONGYI_UUID_STR_LEN, strlen(task_id));
    TEST_ASSERT_EQUAL('4', task_id[14]);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_tongyi_send_audio(handle, (const uint8_t *)"x", 1));
    TEST_ESP_OK(esp_tongyi_stop(handle));
    TEST_ESP_OK(esp_tongyi_stop(handle));
    TEST_ESP_OK(esp_tongyi_destroy(handle));
}

TEST_CASE("uuid v4 and base64 size checks", "[esp_tongyi]")
{
    char uuid[ESP_TONGYI_UUID_BUF_LEN];
    TEST_ESP_OK(esp_tongyi_generate_uuid(uuid, sizeof(uuid)));
    TEST_ASSERT_EQUAL('4', uuid[14]);
    TEST_ASSERT_TRUE(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b');

    size_t out_len = 0;
    TEST_ESP_OK(esp_tongyi_base64_encoded_size(3, &out_len));
    TEST_ASSERT_EQUAL(5, out_len);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_tongyi_base64_encoded_size(SIZE_MAX, &out_len));

    char buf[8];
    const uint8_t data[] = { 'A', 'B', 'C' };
    TEST_ESP_OK(esp_tongyi_base64_encode(data, sizeof(data), buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("QUJD", buf);
}
