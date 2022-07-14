/* Muxer test example to mux AV data into media file

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "audio_event_iface.h"
#include "esp_peripherals.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "esp_log.h"
#include "av_muxer.h"

#define MUXER_FILE_DURATION          (30000) // Unit ms
#define MUXER_FILE_AUDIO_SAMPLE_RATE (22050)
#define MUXER_FILE_VIDEO_QUALITY     (MUXER_VIDEO_QUALITY_HVGA)

static const char* TAG = "AV Muxer";
static int total_data_size = 0;

static int mount_sdcard(esp_periph_set_handle_t set)
{
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    return 0;
}

static int _data_received(uint8_t* data, uint32_t size, void* ctx)
{
    total_data_size += size;
    return 0;
}

static int muxer_format_test(char* fmt, uint32_t duration)
{
    av_muxer_cfg_t cfg = {
        .audio_sample_rate = MUXER_FILE_AUDIO_SAMPLE_RATE,
        .data_cb = _data_received,
        .quality = MUXER_FILE_VIDEO_QUALITY,
        .save_to_file = true,
        .muxer_flag = AV_MUXER_AUDIO_FLAG | AV_MUXER_VIDEO_FLAG,
    };
    strncpy(cfg.fmt, fmt, sizeof(cfg.fmt));
    ESP_LOGI(TAG, "Start muxer to %s", fmt);
    int ret = av_muxer_start(&cfg);
    if (ret != 0) {
        ESP_LOGI(TAG, "Fail to start muxer ret %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Start muxer success");
    while (av_muxer_get_pts() < duration) {
        media_lib_thread_sleep(1000);
        if (av_muxer_running() == false) {
            break;
        }
        if (total_data_size) {
            ESP_LOGI(TAG, "%d Write size %d", av_muxer_get_pts(), total_data_size);
        }
        total_data_size = 0;
    }
    ESP_LOGI(TAG, "Start to stop muxer");
    av_muxer_stop();
    return 0;
}

static void muxer_simple_test(void* arg)
{
    muxer_format_test("mp4", MUXER_FILE_DURATION);
    muxer_format_test("flv", MUXER_FILE_DURATION);
    muxer_format_test("ts", MUXER_FILE_DURATION);
    media_lib_thread_destroy(NULL);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    media_lib_add_default_adapter();
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    mount_sdcard(set);
    media_lib_thread_handle_t muxer_thread;
    media_lib_thread_create(&muxer_thread, "muxer test", muxer_simple_test, NULL, 4 * 1024, 20, 0);
}
