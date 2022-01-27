/* Read battery voltage

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "audio_mem.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "battery_service.h"

/**
 * @brief Battery adc configure
 */
typedef struct {
    int     unit;   /*!< ADC unit, see `adc_unit_t` */
    int     chan;   /*!< ADC channel, see `adc_channel_t` */
    int     width;  /*!< ADC width, see `adc_channel_t` */
    int     atten;  /*!< ADC atten, see `adc_atten_t` */
    int     v_ref;  /*!< default vref` */
    float   ratio;  /*!< divider ratio` */
} vol_adc_param_t;

static const char *TAG = "BATTERY_EXAMPLE";

static bool adc_init(void *user_data)
{
    vol_adc_param_t *adc_cfg = (vol_adc_param_t *)user_data;
    adc1_config_width(adc_cfg->width);
    adc1_config_channel_atten(adc_cfg->chan, adc_cfg->atten);
    return true;
}

static bool adc_deinit(void *user_data)
{
    return true;
}

static int vol_read(void *user_data)
{
#define ADC_SAMPLES_NUM (10)

    vol_adc_param_t *adc_cfg = (vol_adc_param_t *)user_data;

    uint32_t data[ADC_SAMPLES_NUM] = { 0 };
    uint32_t sum = 0;
    int tmp = 0;

    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_characterize(adc_cfg->unit, adc_cfg->atten, adc_cfg->width, adc_cfg->v_ref, &characteristics);

    for (int i = 0; i < ADC_SAMPLES_NUM; ++i) {
        esp_adc_cal_get_voltage(adc_cfg->chan, &characteristics, &data[i]);
    }

    for (int j = 0; j < ADC_SAMPLES_NUM - 1; j++) {
        for (int i = 0; i < ADC_SAMPLES_NUM - j - 1; i++) {
            if (data[i] > data[i + 1]) {
                tmp = data[i];
                data[i] = data[i + 1];
                data[i + 1] = tmp;
            }
        }
    }
    for (int num = 1; num < ADC_SAMPLES_NUM - 1; num++)
        sum += data[num];
    return (int)((sum / (ADC_SAMPLES_NUM - 2)) * adc_cfg->ratio);
}

static esp_err_t battery_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    if (evt->type == BAT_SERV_EVENT_VOL_REPORT) {
        int voltage = (int)evt->data;
        ESP_LOGI(TAG, "got voltage %d", voltage);
    } else if (evt->type == BAT_SERV_EVENT_BAT_FULL) {
        int voltage = (int)evt->data;
        ESP_LOGW(TAG, "battery full %d", voltage);
    } else if (evt->type == BAT_SERV_EVENT_BAT_LOW) {
        int voltage = (int)evt->data;
        ESP_LOGE(TAG, "battery low %d", voltage);
    } else {
        ESP_LOGW(TAG, "unrecognized event %d", evt->type);
    }
    return ESP_OK;
}

void app_main(void)
{
    vol_adc_param_t adc_cfg = {
        .unit = ADC_UNIT_1,
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
        .chan = ADC1_CHANNEL_5,
        .ratio = 4.0,
#else
        .chan = ADC1_CHANNEL_1,
        .ratio = 2.0,
#endif
#ifdef CONFIG_IDF_TARGET_ESP32S2
        .width = ADC_WIDTH_BIT_13,
#else
        .width = ADC_WIDTH_BIT_12,
#endif
        .atten = ADC_ATTEN_11db,
#ifdef CONFIG_IDF_TARGET_ESP32
        .v_ref = 1100,
#else
        .v_ref = 0,
#endif
    };

    vol_monitor_param_t vol_monitor_cfg = {
        .init = adc_init,
        .deinit = adc_deinit,
        .vol_get = vol_read,
        .read_freq = 2,
        .report_freq = 2,
        .vol_full_threshold = CONFIG_VOLTAGE_OF_BATTERY_FULL,
        .vol_low_threshold = CONFIG_VOLTAGE_OF_BATTERY_LOW,
    };
    vol_monitor_cfg.user_data = audio_calloc(1, sizeof(vol_adc_param_t));
    AUDIO_MEM_CHECK(TAG, vol_monitor_cfg.user_data, return);
    memcpy(vol_monitor_cfg.user_data, &adc_cfg, sizeof(vol_adc_param_t));

    ESP_LOGI(TAG, "[1.0] create battery service");
    battery_service_config_t config = BATTERY_SERVICE_DEFAULT_CONFIG();
    config.evt_cb = battery_service_cb;
    config.vol_monitor = vol_monitor_create(&vol_monitor_cfg);
    periph_service_handle_t battery_service = battery_service_create(&config);

    ESP_LOGI(TAG, "[1.1] start battery service");
    if (periph_service_start(battery_service) != ESP_OK) {
        goto exit;
    }

    ESP_LOGI(TAG, "[1.2] start battery voltage report");
    battery_service_vol_report_switch(battery_service, true);

    int wait_sec = 60;
    while (wait_sec--) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGV(TAG, "battery checkout remain %d", wait_sec);
    }

    ESP_LOGI(TAG, "[1.3] change battery voltage report freqency");
    battery_service_set_vol_report_freq(battery_service, 1);
    wait_sec = 60;
    while (wait_sec--) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGV(TAG, "battery checkout remain %d", wait_sec);
    }

    ESP_LOGI(TAG, "[2.0] stop battery voltage report");
    battery_service_vol_report_switch(battery_service, false);
exit:
    ESP_LOGI(TAG, "[2.1] destroy battery service");
    periph_service_destroy(battery_service);
    vol_monitor_destroy(config.vol_monitor);
    free(vol_monitor_cfg.user_data);
}
