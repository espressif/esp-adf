/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_event_iface.h"
#include "esp_log.h"
#include "esp_err.h"

#include "unity.h"
#include "nvs_flash.h"
#include "audio_mem.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "board.h"

static const char *TAG = "ESP_PERIPH_TEST";

#define TEST_PERIPHERALS_MEMORY_LEAK_TIMES  1000

static void periph_adc_button_test(void)
{
    ESP_LOGI(TAG, "Set up peripherals handle");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    TEST_ASSERT_NOT_NULL(set);

    ESP_LOGI(TAG, "Register ADC button to peripherals");
    periph_adc_button_cfg_t adc_btn_cfg = PERIPH_ADC_BUTTON_DEFAULT_CONFIG();
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_btn_tag.total_steps = 6;
#if CONFIG_IDF_TARGET_ESP32S3
    adc_btn_tag.adc_ch = ADC_CHANNEL_4;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 3100};
#elif CONFIG_IDF_TARGET_ESP32S2
    adc_btn_tag.adc_ch = ADC_CHANNEL_5;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 2510};
#elif CONFIG_IDF_TARGET_ESP32C3
    adc_btn_tag.adc_ch = ADC_CHANNEL_2;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 2700};
#elif CONFIG_IDF_TARGET_ESP32P4
    adc_btn_tag.adc_ch = ADC_CHANNEL_6;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 3100};
#else
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 3100};
#endif
    adc_btn_tag.adc_level_step = btn_array;
    adc_btn_cfg.arr = &adc_btn_tag;
    adc_btn_cfg.arr_size = 1;
    esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
    TEST_ASSERT_NOT_NULL(adc_btn_handle);

    TEST_ASSERT_FALSE(esp_periph_start(set, adc_btn_handle));

    ESP_LOGI(TAG, "Set up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    TEST_ASSERT_NOT_NULL(evt);

    ESP_LOGI(TAG, "Listening event from peripherals");
    TEST_ASSERT_FALSE(audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt));

    ESP_LOGI(TAG, "Test start, please press buttons on the board ... ");
    while (1) {
        audio_event_iface_msg_t msg;
        TEST_ASSERT_FALSE(audio_event_iface_listen(evt, &msg, portMAX_DELAY));
        ESP_LOGI(TAG, "action: %d, act id: %d", msg.cmd, (int)msg.data);
        if ((int)msg.data == 0) {
            ESP_LOGW(TAG, "press id 0, quit test");
            break;
        }
    }

    ESP_LOGI(TAG, "Quit test, release all resources");
    TEST_ASSERT_FALSE(esp_periph_set_stop_all(set));
    TEST_ASSERT_FALSE(audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt));
    TEST_ASSERT_FALSE(audio_event_iface_destroy(evt));
    TEST_ASSERT_FALSE(esp_periph_set_destroy(set));
}

TEST_CASE("adc button test", "[peripherals]")
{
    AUDIO_MEM_SHOW(TAG);
    periph_adc_button_test();
    vTaskDelay(1000 / portTICK_RATE_MS);
    AUDIO_MEM_SHOW(TAG);
}

TEST_CASE("esp_periph memory test", "[peripherals]")
{
    AUDIO_MEM_SHOW(TAG);
    int test_count = TEST_PERIPHERALS_MEMORY_LEAK_TIMES;
    while (test_count--) {
        printf("-------Residual times: %d -------\n", test_count);
        ESP_LOGI(TAG, "Init ADC button to peripherals");
        periph_adc_button_cfg_t adc_btn_cfg = PERIPH_ADC_BUTTON_DEFAULT_CONFIG();
        adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
        adc_btn_cfg.arr = &adc_btn_tag;
        esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
        TEST_ASSERT_NOT_NULL(adc_btn_handle);

        ESP_LOGI(TAG, "Destroy ADC button to peripherals");
        TEST_ASSERT_FALSE(esp_periph_destroy(adc_btn_handle));
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
    AUDIO_MEM_SHOW(TAG);
}
