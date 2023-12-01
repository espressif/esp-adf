/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_peripherals.h"
#include "nvs_flash.h"
#include "periph_wifi.h"
#include "audio_mem.h"
#include "audio_url.h"
#include "coredump_upload_service.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "coredump_example";

#define CUSTOMIZE_POST (1)

bool do_http_post(char *url, uint8_t *data, size_t len)
{
    size_t max_len = strlen(url) + 512;
    char *url_str = audio_calloc(1, max_len);
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    snprintf(url_str, max_len, "%s?app_version=%s&proj_name=%s&idf_version=%s",
                                url,
                                app_desc->version,
                                app_desc->project_name,
                                app_desc->idf_ver);
    char *url_final = audio_url_encode(url_str);

    esp_http_client_config_t config = {
        .url = url_final,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    esp_http_client_set_header(http_client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(http_client, (const char *)data, (int)len);
    int response = 0;
    if (esp_http_client_perform(http_client) == ESP_OK) {
        response = esp_http_client_get_status_code(http_client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", response, (int)esp_http_client_get_content_length(http_client));
    } else {
        ESP_LOGE(TAG, "Post failed");
    }
    esp_http_client_cleanup(http_client);
    free(url_str);
    free(url_final);
    return response == 200 ? true : false;
}

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    esp_err_t result = ESP_FAIL;

    if (coredump_need_upload()) {
        coredump_upload_service_config_t coredump_upload_service_cfg = COREDUMP_UPLOAD_SERVICE_DEFAULT_CONFIG();
#if CUSTOMIZE_POST
        coredump_upload_service_cfg.do_post = do_http_post;
#endif
        periph_service_handle_t uploader = coredump_upload_service_create(&coredump_upload_service_cfg);
        if (NULL != uploader) {
            result = coredump_upload(uploader, CONFIG_COREDUMP_UPLOAD_URI);
        }
        periph_service_destroy(uploader);
    }
    ESP_LOGI(TAG, "result %d", result);
    if (result == ESP_FAIL) {
        __asm__ __volatile__("ill");
    }
}
