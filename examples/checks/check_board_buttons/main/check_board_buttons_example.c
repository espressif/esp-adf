/* Check operation of ADC button peripheral in board

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"

static const char *TAG = "CHECK_BOARD_BUTTONS";

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "[ * ] input key id is %d, %d", (int)evt->data, evt->type);
    const char *key_types[INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE + 1] = {"UNKNOWN", "CLICKED", "CLICK RELEASED", "PRESSED", "PRESS RELEASED"};
    switch ((int)evt->data) {
        case INPUT_KEY_USER_ID_REC:
            ESP_LOGI(TAG, "[ * ] [Rec] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_SET:
            ESP_LOGI(TAG, "[ * ] [SET] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_PLAY:
            ESP_LOGI(TAG, "[ * ] [Play] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_MODE:
            ESP_LOGI(TAG, "[ * ] [MODE] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_VOLDOWN:
            ESP_LOGI(TAG, "[ * ] [Vol-] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_VOLUP:
            ESP_LOGI(TAG, "[ * ] [Vol+] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_MUTE:
            ESP_LOGI(TAG, "[ * ] [MUTE] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_CAPTURE:
            ESP_LOGI(TAG, "[ * ] [CAPTURE] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_MSG:
            ESP_LOGI(TAG, "[ * ] [MSG] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_BATTERY_CHARGING:
            ESP_LOGI(TAG, "[ * ] [BATTERY_CHARGING] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_WAKEUP:
            ESP_LOGI(TAG, "[ * ] [WAKEUP] KEY %s", key_types[evt->type]);
            break;
        case INPUT_KEY_USER_ID_COLOR:
            ESP_LOGI(TAG, "[ * ] [COLOR] KEY %s", key_types[evt->type]);
            break;
        default:
            ESP_LOGE(TAG, "User Key ID[%d] does not support", (int)evt->data);
            break;
    }

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 2 ] Initialize Button peripheral with board init");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);

    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, NULL);

    ESP_LOGW(TAG, "[ 4 ] Waiting for a button to be pressed ...");
}
