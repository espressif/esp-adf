#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_audio_device_info.h"
#include "lightduer_connagent.h"
#include "lightduer_dcs.h"
#include "lightduer_dipb_data_handler.h"
#include "lightduer_dlp.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_voice.h"

#include "esp_log.h"

const static char *TAG          = "duer_handler";
static int32_t     cur_voice_id = 0;

duer_status_t duer_dcs_input_text_handler(const char *text, const char *type)
{
    ESP_LOGW(TAG, "duer_dcs_input_text_handler: %s: %s", type, text);
    return DUER_OK;
}

duer_status_t duer_dcs_render_card_handler(baidu_json *payload)
{
    baidu_json *type = NULL;
    baidu_json *content = NULL;
    duer_status_t ret = DUER_OK;

    do {
        if (!payload) {
            ret = DUER_ERR_FAILED;
            break;
        }

        type = baidu_json_GetObjectItem(payload, "type");
        if (!type) {
            ret = DUER_ERR_FAILED;
            break;
        }

        if (strcmp("TextCard", type->valuestring) == 0) {
            content = baidu_json_GetObjectItem(payload, "content");
            if (!content) {
                ret = DUER_ERR_FAILED;
                break;
            }
            ESP_LOGW(TAG, "Render card content: %s", content->valuestring);
        }
    } while (0);

    return ret;
}

duer_status_t duer_dcs_render_stream_card_handler(const duer_dcs_render_stream_card_t *stream_card_payload)
{
    duer_status_t ret = DUER_OK;

    do {
        if (!stream_card_payload) {
            ret = DUER_ERR_FAILED;
            break;
        }
        if (stream_card_payload->answer) {
            ESP_LOGW(TAG, "Render stream card answer: %s\n", stream_card_payload->answer);
        }
        ESP_LOGW(TAG, "Render stream card index: %d\n", stream_card_payload->index);
        if (stream_card_payload->part) {
            ESP_LOGW(TAG, "Render stream card part: %s\n", stream_card_payload->part);
        }
        if (stream_card_payload->tts) {
            ESP_LOGW(TAG, "Render stream card tts: %s\n", stream_card_payload->tts);
        }
        ESP_LOGW(TAG, "Render stream card end: %d\n", stream_card_payload->is_end);
    } while (0);

    return ret;
}

void duer_dcs_set_voice_handler(int voice_id)
{
    ESP_LOGI(TAG, "set voice id to [%d]", voice_id);
    cur_voice_id = voice_id;
    // TODO: Save the configuration and load when init
}

int duer_dcs_get_voice_id(void)
{
    return cur_voice_id;
}
