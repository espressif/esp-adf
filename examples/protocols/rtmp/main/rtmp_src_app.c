/* RTMP source application to fetch data from server and callback in FLV container

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_timer.h"
#include "rtmp_src_app.h"
#include "esp_rtmp_src.h"
#include "rtmp_app_setting.h"
#include "esp_log.h"

#define TAG                     "RTMP Src_App"

#define RTMP_SRC_CHUNK_SIZE     (4 * 1024)
#define RTMP_SRC_RING_FIFO_SIZE (500 * 1024)
#define RTMP_SRC_FILE_PATH      "/sdcard/src.flv"

static int write_file(void *data, int size)
{
    static FILE *fp = NULL;
    if (fp == NULL && size > 0) {
        fp = fopen(RTMP_SRC_FILE_PATH, "wb");
    }
    if (fp == NULL) {
        ESP_LOGE(TAG, "Fail to open file %s for write", RTMP_SRC_FILE_PATH);
        return -1;
    }
    if (data == NULL || size == 0) {
        fclose(fp);
    } else {
        if (fwrite(data, 1, size, fp) != size) {
            ESP_LOGE(TAG, "Fail to write file");
            return -1;
        }
    }
    return 0;
}

int rtmp_src_app_run(char *uri, uint32_t duration)
{
    int ret;
    media_lib_tls_cfg_t ssl_cfg;
    rtmp_src_cfg_t cfg = {
        .url = uri,
        .chunk_size = RTMP_SRC_CHUNK_SIZE,
        .fifo_size = RTMP_SRC_RING_FIFO_SIZE,
        .thread_cfg = {.priority = 10, .stack_size = 5*1024},
    };
    if (strncmp(uri, "rtmps://", 8) == 0) {
        cfg.ssl_cfg = &ssl_cfg;
        rtmp_setting_get_client_ssl_cfg(uri, &ssl_cfg);
    }
    rtmp_src_handle_t *rtmp_src = esp_rtmp_src_open(&cfg);
    if (rtmp_src == NULL) {
        ESP_LOGE(TAG, "Fail to open rtmp src");
        return -1;
    }

    uint32_t start_time = esp_timer_get_time() / 1000;
    ret = esp_rtmp_src_connect(rtmp_src);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to connect to server");
    } else {
        uint8_t *data = (uint8_t *) malloc(1024);
        if (data) {
            while (rtmp_setting_get_allow_run()) {
                ret = esp_rtmp_src_read(rtmp_src, data, sizeof(data));
                if (ret == 0) {
                    ret = write_file(data, sizeof(data));
                    if (ret != 0) {
                        break;
                    }
                } else {
                    break;
                }
                uint32_t cur_time = esp_timer_get_time() / 1000;
                if (cur_time > duration && cur_time > start_time + duration) {
                    break;
                }
            }
            free(data);
        }
    }
    // sync data and close
    write_file(NULL, 0);
    ret = esp_rtmp_src_close(rtmp_src);
    ESP_LOGI(TAG, "Close rtmp return %d\n", ret);
    return ret;
}
