#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "input_key_service.h"
#include "esp_peripherals.h"
#include "periph_service.h"
#include "periph_button.h"
#include "periph_touch.h"
#include "periph_adc_button.h"
#include "board.h"
#include "unity.h"

static const char *TAG = "TEST_INPUT_KEY_SERVICE";

static esp_periph_set_handle_t set = NULL;

static esp_err_t test_input_key_service_callback(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(evt);
    ESP_LOGI(TAG, "type=>%d, source=>%d, data=>%d, len=>%d", evt->type, (int)evt->source, (int)evt->data, evt->len);
    return ESP_OK;
}

static periph_service_handle_t test_input_key_service_create()
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);
    TEST_ASSERT_NOT_NULL(set);

#if (CONFIG_ESP_LYRAT_V4_3_BOARD || CONFIG_ESP_LYRAT_V4_2_BOARD)
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_rec_id()) | (1ULL << get_input_mode_id()),
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    TEST_ASSERT_NOT_NULL(button_handle);
    TEST_ASSERT_FALSE(esp_periph_start(set, button_handle));

    periph_touch_cfg_t touch_cfg = {
        .touch_mask = BIT(get_input_set_id()) | BIT(get_input_play_id()) | BIT(get_input_volup_id()) | BIT(get_input_voldown_id()),
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_handle = periph_touch_init(&touch_cfg);
    TEST_ASSERT_NOT_NULL(touch_handle);
    TEST_ASSERT_FALSE(esp_periph_start(set, touch_handle));
#endif

#if (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
    periph_adc_button_cfg_t adc_btn_cfg = {0};
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_btn_cfg.arr = &adc_btn_tag;
    adc_btn_cfg.arr_size = 1;
    esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
    TEST_ASSERT_NOT_NULL(adc_btn_handle);
    TEST_ASSERT_FALSE(esp_periph_start(set, adc_btn_handle));
#endif

    input_key_service_info_t input_info[] = INPUT_KEY_DEFAULT_INFO();
    periph_service_handle_t input_key_handle = input_key_service_create(set);
    TEST_ASSERT_NOT_NULL(input_key_handle);
    TEST_ASSERT_FALSE(input_key_service_add_key(input_key_handle, input_info, INPUT_KEY_NUM));
    TEST_ASSERT_FALSE(periph_service_set_callback(input_key_handle, test_input_key_service_callback, NULL));
    return input_key_handle;
}

static void test_input_key_service_destroy(periph_service_handle_t handle)
{
    TEST_ASSERT_FALSE(periph_service_destroy(handle));
    TEST_ASSERT_FALSE(esp_periph_set_destroy(set));
    set = NULL;
}

static void test_input_key_service_start(periph_service_handle_t handle)
{
    TEST_ASSERT_FALSE(periph_service_start(handle));
}

static void test_input_key_service_stop(periph_service_handle_t handle)
{
    TEST_ASSERT_FALSE(periph_service_stop(handle));
}

TEST_CASE("Operate input_key_service", "[input_key_service]")
{
    periph_service_handle_t input_key_handle = test_input_key_service_create();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    test_input_key_service_stop(input_key_handle);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    test_input_key_service_start(input_key_handle);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    test_input_key_service_destroy(input_key_handle);
}

TEST_CASE("Operating input key service quickly", "[input_key_service]")
{
    periph_service_handle_t input_key_handle = test_input_key_service_create();
    test_input_key_service_start(input_key_handle);
    test_input_key_service_stop(input_key_handle);
    test_input_key_service_stop(input_key_handle);
    test_input_key_service_destroy(input_key_handle);
}

TEST_CASE("Illegal calling of functions", "[input_key_service]")
{
    TEST_ASSERT_TRUE(periph_service_start(NULL));
    TEST_ASSERT_TRUE(periph_service_stop(NULL));
    TEST_ASSERT_TRUE(periph_service_destroy(NULL));
    TEST_ASSERT_FALSE(input_key_service_create(NULL, NULL));
}
