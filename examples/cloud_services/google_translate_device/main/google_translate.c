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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mbedtls/base64.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "google_translate.h"
#include "json_utils.h"

static const char *TAG = "GOOGLE_TRANSLATE";

#define GOOGLE_TRANSLATE_ENDPOINT   "https://translation.googleapis.com/language/translate/v2?key="
#define GOOGLE_TRANSLATE_TEMPLATE   "{\"source\":\"%s\", \"target\": \"%s\", \"format\":\"text\", \"q\":\"%s\"}"
#define MAX_TRANSLATE_BUFFER (2048)

char *google_translate(const char *text, const char *lang_from, const char *lang_to, const char *api_key)
{
    char *response_text = NULL;
    char *uri = NULL;
    char *post_buffer = NULL;
    char *data_buf = NULL;
    esp_http_client_handle_t client = NULL;

    asprintf(&uri, "%s%s", GOOGLE_TRANSLATE_ENDPOINT, api_key);
    if (uri == NULL) {
        return NULL;
    }
    esp_http_client_config_t translate_config = {
        .url = uri,
        .method = HTTP_METHOD_POST,
    };
    int post_len = asprintf(&post_buffer,
                            GOOGLE_TRANSLATE_TEMPLATE,
                            lang_from,
                            lang_to,
                            text);
    if (post_buffer == NULL) {
        goto exit_translate;
    }
    client = esp_http_client_init(&translate_config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (esp_http_client_open(client, post_len) != ESP_OK) {
        ESP_LOGE(TAG, "Error opening connection");
        goto exit_translate;
    }
    int write_len = esp_http_client_write(client, post_buffer, post_len);
    ESP_LOGI(TAG, "Need to write %d, written %d", post_len, write_len);

    int data_length = esp_http_client_fetch_headers(client);
    if (data_length <= 0) {
        data_length = MAX_TRANSLATE_BUFFER;
    }
    data_buf = malloc(data_length + 1);
    if(data_buf == NULL) {
        goto exit_translate;
    }
    data_buf[data_length] = '\0';
    int rlen = esp_http_client_read(client, data_buf, data_length);
    data_buf[rlen] = '\0';
    ESP_LOGI(TAG, "read = %s", data_buf);
    response_text = json_get_token_value(data_buf, "translatedText");
    if (response_text) {
        ESP_LOGI(TAG, "Response text = %s", response_text);
    }
exit_translate:
    free(post_buffer);
    free(data_buf);
    free(uri);
    esp_http_client_cleanup(client);
    return response_text;
}
