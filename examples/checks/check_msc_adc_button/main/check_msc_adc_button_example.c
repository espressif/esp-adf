/* Check operation of ADC button peripheral in ESP32-LyraTD-MSC board

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"

static const char *TAG = "CHECK_MSC_ADC_BUTTON";


void app_main(void)
{
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize ADC Button peripheral");
    periph_adc_button_cfg_t adc_button_cfg = { 0 };
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_button_cfg.arr = &adc_btn_tag;
    adc_button_cfg.arr_size = 1;

    ESP_LOGI(TAG, "[1.2] Start ADC Button peripheral");
    esp_periph_handle_t adc_button_periph = periph_adc_button_init(&adc_button_cfg);
    esp_periph_start(adc_button_periph);

    ESP_LOGI(TAG, "[ 2 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGW(TAG, "[ 3 ] Waiting for a button pressed ...");


    while (1) {
        char *btn_states[] = {"idle", "click", "pressed", "released"};
    
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == PERIPH_ID_ADC_BTN) {
            int button_id = msg.data_len;  // button id is sent as data_len
            int state     = msg.cmd;       // button state is sent as cmd
            switch (button_id) {
                case USER_KEY_ID0:
                    ESP_LOGI(TAG, "[ * ] Button SET %s", btn_states[state]);
                    break;
                case USER_KEY_ID1:
                    ESP_LOGI(TAG, "[ * ] Button PLAY %s", btn_states[state]);
                    break;
                case USER_KEY_ID2:
                    ESP_LOGI(TAG, "[ * ] Button REC %s", btn_states[state]);
                    break;
                case USER_KEY_ID3:
                    ESP_LOGI(TAG, "[ * ] Button MODE %s", btn_states[state]);
                    break;
                case USER_KEY_ID4:
                    ESP_LOGI(TAG, "[ * ] Button VOL- %s", btn_states[state]);
                    break;
                case USER_KEY_ID5:
                    ESP_LOGI(TAG, "[ * ] Button VOL+ %s", btn_states[state]);
                    break;
                default:
                    ESP_LOGE(TAG, "[ * ] Not supported button id: %d)", button_id);
            }
        }
    }

    ESP_LOGI(TAG, "[ 4 ] Stop & destroy all peripherals and event interface");
    esp_periph_destroy();
    audio_event_iface_destroy(evt);
}
