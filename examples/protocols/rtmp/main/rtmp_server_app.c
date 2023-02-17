/* RTMP server application

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_timer.h"
#include "rtmp_server_app.h"
#include "esp_rtmp_server.h"
#include "media_lib_os.h"
#include "rtmp_app_setting.h"
#include "esp_log.h"

#define TAG                          "RTMP Server_App"

#define RTMP_SERVER_CHUNK_SIZE        (4 * 1024)
#define RTMP_SERVER_CLIENT_CACHE_SIZE (300*1024)
#define RTMP_SERVER_MAX_CLIENT        (6)

static uint16_t get_port(char *uri)
{
    uint16_t port = 1935;
    char *p = uri;
    while (1) {
        p = strchr(p, ':');
        if (p == NULL) {
            break;
        }
        if (*p) {
            p++;
            if (*p >= '0' && *p <= '9') {
                port = (uint16_t) atoi(p);
                break;
            }
        }
    }
    return port;
}

static char *get_app(char *url)
{
    while (1) {
        url = strchr(url, '/');
        if (url) {
            if (*(url - 1) == ':' || *(url - 1) == '/') {
                url++;
                continue;
            }
            url++;
            char *nxt = strchr(url, '/');
            int len = nxt ? nxt - url : strlen(url);
            char *app = (char *) media_lib_malloc(len + 1);
            if (app) {
                memcpy(app, url, len);
                app[len] = 0;
            }
            return app;
        }
        break;
    }
    return NULL;
}

static bool rtmp_server_verify(char *stream_key, void *ctx)
{
    return true;
}

int rtmp_server_app_run(char *uri, uint32_t duration)
{
    int ret;
    uint16_t port = get_port(uri);
    char *app_name = get_app(uri);
    if (app_name == NULL) {
        ESP_LOGE(TAG, "Bad uri setting %s", uri);
        return -1;
    }
    media_lib_tls_server_cfg_t ssl_cfg;
    rtmp_server_cfg_t cfg = {
        .app_name = app_name,
        .chunk_size = RTMP_SERVER_CHUNK_SIZE,
        .port = port,
        .auth_cb = rtmp_server_verify,
        .max_clients = RTMP_SERVER_MAX_CLIENT,
        .client_cache_size = RTMP_SERVER_CLIENT_CACHE_SIZE,
        .thread_cfg = {.priority = 10, .stack_size = 5*1024},
    };
    if (strncmp(uri, "rtmps://", 8) == 0) {
        cfg.ssl_cfg = &ssl_cfg;
        rtmp_setting_get_server_ssl_cfg(&ssl_cfg);
    }
    ESP_LOGI(TAG, "Start run server on %s", uri);
    rtmp_server_handle_t server = esp_rtmp_server_open(&cfg);
    media_lib_free(app_name);
    if (server == NULL) {
        ESP_LOGE(TAG, "Fail to open rtmp server");
        return -1;
    }
    ret = esp_rtmp_server_setup(server);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to Setup server");
    } else {
        uint32_t start_time = esp_timer_get_time() / 1000;
        while (rtmp_setting_get_allow_run()) {
            media_lib_thread_sleep(1000);
            uint32_t cur_time = esp_timer_get_time() / 1000;
            if (cur_time > duration && cur_time > start_time + duration) {
                break;
            }
        }
    }
    ret = esp_rtmp_server_close(server);
    ESP_LOGI(TAG, "Close rtmp return %d\n", ret);
    return ret;
}
