/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include <pthread.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_pipeline.h"
#include "esp_log.h"
#include "esp_err.h"
#include "fatfs_stream.h"
#include "sdcard_service.h"

static const char *TAG = "FATFS_STREAM_TEST";

esp_err_t evt_process(audio_event_iface_msg_t *msg, void *context)
{
    ESP_LOGI(TAG, "receive evt msg cmd = %d, source addr = %x, type = %d", msg->cmd, (int)msg->source, msg->source_type);
    if (msg->cmd == AEL_MSG_CMD_STOP) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

TEST_CASE("fatfs_stream", "esp-adf")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_EVENT", ESP_LOG_VERBOSE);
    esp_log_level_set("FATFS_STREAM", ESP_LOG_VERBOSE);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_VERBOSE);
    esp_log_level_set("FATFS_STREAM_TEST", ESP_LOG_VERBOSE);

    sdcard_service_cfg_t sdcfg = SDCARD_DEFAULT_CONFIG();
    audio_element_handle_t sdcard_serv = sdcard_service_init(&sdcfg);
    TEST_ASSERT_NOT_NULL(sdcard_serv);
    audio_element_run(sdcard_serv);
    audio_element_resume(sdcard_serv, 0, 0);


    audio_element_handle_t fatfs_rd;
    fatfs_stream_cfg_t fatfs_cfg = {
        .type = AUDIO_STREAM_READER,
        .root_path = "/sdcard",
    };

    fatfs_rd = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(fatfs_rd, "/sdcard/test.wav");

    audio_element_run(fatfs_rd);
    audio_element_resume(fatfs_rd, 0, 0);
    vTaskDelay(10000/portTICK_RATE_MS);
    audio_element_pause(fatfs_rd);
    vTaskDelay(1000/portTICK_RATE_MS);
    audio_element_resume(fatfs_rd, 0, 0);
    vTaskDelay(5000/portTICK_RATE_MS);
    audio_element_stop(fatfs_rd);
    vTaskDelay(1000/portTICK_RATE_MS);
    audio_element_run(fatfs_rd);
    audio_element_resume(fatfs_rd, 0, 0);
    vTaskDelay(5000/portTICK_RATE_MS);
    audio_element_stop(fatfs_rd);

    audio_element_stop(sdcard_serv);

    audio_element_wait_for_stop(fatfs_rd);
    audio_element_wait_for_stop(fatfs_rd);

    audio_element_deinit(fatfs_rd);
    audio_element_deinit(sdcard_serv);
}
