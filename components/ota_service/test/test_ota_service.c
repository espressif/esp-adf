/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ota_service.h"
#include "ota_proc_default.h"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_types.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "ff.h"
#include "board.h"

#include "unity.h"

static const char *TAG = "ota_test";
static EventGroupHandle_t events = NULL;

#define OTA_FINISH (BIT0)

static ota_service_err_reason_t do_need_upgrade(void *handle, ota_node_attr_t *node)
{
    return OTA_SERV_ERR_REASON_SUCCESS;
}

static ota_service_err_reason_t no_need_upgrade(void *handle, ota_node_attr_t *node)
{
    return OTA_SERV_ERR_REASON_UNKNOWN;
}

static esp_err_t ota_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    if (evt->type == OTA_SERV_EVENT_TYPE_RESULT) {
        ota_result_t *result_data = evt->data;
        ESP_LOGI(TAG, "OTA EVENT RESULT: 0x%X", result_data->result);
    } else if (evt->type == OTA_SERV_EVENT_TYPE_FINISH) {
        ESP_LOGI(TAG, "OTA EVENT FIN");
        xEventGroupSetBits(events, OTA_FINISH);
    }

    return ESP_OK;
}

static esp_err_t test_ota_proc(ota_upgrade_ops_t *upgrade_list, uint32_t list_len)
{
    if (upgrade_list == NULL || list_len == 0) {
        return ESP_FAIL;
    }
    events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(events);

    ota_service_config_t ota_service_cfg = OTA_SERVICE_DEFAULT_CONFIG();
    ota_service_cfg.evt_cb = ota_service_cb;
    periph_service_handle_t ota_service = ota_service_create(&ota_service_cfg);
    TEST_ASSERT_NOT_NULL(ota_service);
    TEST_ASSERT(ota_service_set_upgrade_param(ota_service,
                                            upgrade_list,
                                            list_len) == ESP_OK);

    TEST_ASSERT(periph_service_start(ota_service) == ESP_OK);

    xEventGroupWaitBits(events, OTA_FINISH, true, false, portMAX_DELAY);
    periph_service_destroy(ota_service);
    vEventGroupDelete(events);
    return ESP_OK;
}

static void sdcard_setup(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.width = 1;

#if CONFIG_IDF_TARGET_ESP32S3
    slot_config.clk = ESP_SD_PIN_CLK;
    slot_config.cmd = ESP_SD_PIN_CMD;
    slot_config.d0 = ESP_SD_PIN_D0;
#else
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
#endif

    TEST_ESP_OK(esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, NULL));
}

static void sdcard_teardown(void)
{
    TEST_ESP_OK(esp_vfs_fat_sdmmc_unmount());
}

TEST_CASE("(service) create and destroy", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_service_config_t ota_service_cfg = OTA_SERVICE_DEFAULT_CONFIG();
    periph_service_handle_t ota_service = ota_service_create(&ota_service_cfg);
    TEST_ASSERT_NOT_NULL(ota_service);
    periph_service_destroy(ota_service);
}

TEST_CASE("(data) ota without partition name", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                NULL,
                NULL,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(data) ota with wrong partition name", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "test",
                NULL,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(data) ota without url", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_test",
                NULL,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(data) ota with wrong url format", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_test",
                "test",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(data) ota with dead url", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_test",
                "file://sdcard/test.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(data) ota with no need upgrade", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    sdcard_setup();
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_test",
                "file://sdcard/data.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
    sdcard_teardown();
}

TEST_CASE("(data) ota process", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    sdcard_setup();
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_test",
                "file://sdcard/data.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = do_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
    sdcard_teardown();
}

TEST_CASE("(app) ota with a app partition name", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                "test",
                NULL,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(app) ota without url", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                NULL,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(app) ota with wrong url format", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                "test",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
}

TEST_CASE("(app) ota with dead url", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    sdcard_setup();
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                "file://sdcard/test.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
    sdcard_teardown();
}

TEST_CASE("(app) ota with no need upgrade", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    sdcard_setup();
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                "file://sdcard/app.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = no_need_upgrade;
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
    sdcard_teardown();
}


TEST_CASE("(app) ota process", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    sdcard_setup();
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                "file://sdcard/app.bin",
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        }
    };
    ota_app_get_default_proc(&upgrade_list[0]);
    TEST_ASSERT(test_ota_proc(upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t)) == ESP_OK);
    sdcard_teardown();
}
