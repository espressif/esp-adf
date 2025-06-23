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

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "math.h"
#include "string.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "adc_button.h"
#include "audio_thread.h"
#include "audio_idf_version.h"

#if ADC_LEGACY_API
#define V_REF              1100
#define ADC_ATTEN_DEFAULT  ADC_ATTEN_DB_11
#else  /* ADC_LEGACY_API */
#if ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && (ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5, 1, 0)))
#define ADC_ATTEN_DEFAULT  ADC_ATTEN_DB_11
#elif (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 1, 0))
#define ADC_ATTEN_DEFAULT  ADC_ATTEN_DB_12
#endif  /* ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0))) */
#endif

#if CONFIG_IDF_TARGET_ESP32S2
#define ADC_WIDTH_DEFAULT  (ADC_BITWIDTH_13)
#else
#define ADC_WIDTH_DEFAULT  (ADC_BITWIDTH_12)
#endif

#define DIAL_VOL_INTERVAL_TIME_MS    150
#define ADC_SAMPLES_NUM              10
#define ADC_SAMPLE_INTERVAL_TIME_MS  20
#define ADC_BTN_INVALID_ID           -1
#define ADC_BTN_INVALID_ACT_ID       -2
#define ADC_BTN_DETECT_TIME_MS       20
#define ADC_BTN_DETECTED_CNT         2
#define ADC_VOL_EMPTY_THRESHOLD      200
#define ADC_VOL_FULL_THRESHOLD       2500
#define ADC_VOL_DIFF_THRESHOLD       80
#define USER_KEY_MAX                 7
#define DESTROY_BIT                  BIT0

typedef struct {
    adc_button_callback  btn_callback;
    adc_btn_list        *head;
    void                *user_data;
    audio_thread_t       audio_thread;
} adc_btn_tag_t;

static char *TAG = "ADC_BTN";

static bool               s_task_flag;
static EventGroupHandle_t s_event_bit;

static const int s_default_step_level[USER_KEY_MAX] = {0, 683, 1193, 1631, 2090, 2578, 3103};

#if !ADC_LEGACY_API
static adc_oneshot_unit_handle_t s_adc_oneshot_handle = NULL;
static adc_cali_handle_t         s_adc_cali_chan_handle = NULL;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
#if ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)) && (!CONFIG_IDF_TARGET_ESP32S3))
            .chan = channel,
#endif  /* ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)) && (!CONFIG_IDF_TARGET_ESP32S3)) */
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif  /* ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED */

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif  /* ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED */

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "EFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Deregister Curve Fitting calibration scheme");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#endif  /* ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED */

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Deregister Line Fitting calibration scheme");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif  /* ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED */
}

#endif  /* !ADC_LEGACY_API */

static int get_adc_voltage(int unit, int channel)
{
    int tmp = 0;
    uint32_t sum = 0;
    uint32_t data[ADC_SAMPLES_NUM] = {0};

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#if ADC_LEGACY_API
    esp_adc_cal_characteristics_t characteristics;
#if CONFIG_IDF_TARGET_ESP32
    esp_adc_cal_characterize(unit, ADC_ATTEN_DEFAULT, ADC_WIDTH_DEFAULT, V_REF, &characteristics);
#else
    esp_adc_cal_characterize(unit, ADC_ATTEN_DEFAULT, ADC_WIDTH_DEFAULT, 0, &characteristics);
#endif  /* CONFIG_IDF_TARGET_ESP32 */
    for (int i = 0; i < ADC_SAMPLES_NUM; ++i) {
        esp_adc_cal_get_voltage(channel, &characteristics, &data[i]);
    }

#else  /* ADC_LEGACY_API */

    int adc_raw[ADC_SAMPLES_NUM];
    int voltage[ADC_SAMPLES_NUM];
    for (int i = 0; i < ADC_SAMPLES_NUM; ++i) {
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc_oneshot_handle, channel, &adc_raw[i]));
        ESP_LOGD(TAG, "ADC[%d] Channel[%d] Raw Data: %d", unit + 1, channel, adc_raw[i]);
        if (s_adc_cali_chan_handle != NULL) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_adc_cali_chan_handle, adc_raw[i], &voltage[i]));
            ESP_LOGD(TAG, "ADC[%d] Channel[%d] Cali Voltage: %d mV", unit + 1, channel, voltage[i]);
            data[i] = voltage[i];
        }
    }
#endif  /* ADC_LEGACY_API */
#endif  /* CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 */
    for (int j = 0; j < ADC_SAMPLES_NUM - 1; j++) {
        for (int i = 0; i < ADC_SAMPLES_NUM - j - 1; i++) {
            if (data[i] > data[i + 1]) {
                tmp = data[i];
                data[i] = data[i + 1];
                data[i + 1] = tmp;
            }
        }
    }
    for (int num = 1; num < ADC_SAMPLES_NUM - 1; num++) {
        sum += data[num];
    }
    return (sum / (ADC_SAMPLES_NUM - 2));
}

static int get_button_id(adc_btn_list *node, int adc)
{
    int m = ADC_BTN_INVALID_ID;
    adc_arr_t *info = &(node->adc_info);
    for (int i = 0; i < info->total_steps; i++) {
        ESP_LOGV(TAG, "Max:%d, ADC:%d, i:%d, %d, %d", info->total_steps, adc, i, info->adc_level_step[i], info->adc_level_step[i + 1]);
        if ((adc > info->adc_level_step[i])
            && (adc <= info->adc_level_step[i + 1])) {
            m = i;
            break;
        }
    }
    return m;
}

static void reset_btn(btn_decription *btn_desc, int btn_num)
{
    memset(btn_desc, 0, sizeof(btn_decription) * btn_num);
    for (int i = 0; i < btn_num; ++i) {
        btn_desc[i].active_id = ADC_BTN_INVALID_ID;
    }
}

static adc_btn_state_t get_adc_btn_state(int adc_value, int act_id, adc_btn_list *node)
{
    adc_btn_state_t st = ADC_BTN_STATE_IDLE;
    adc_arr_t *info = &(node->adc_info);
    btn_decription *btn_desc = node->btn_desc;
    int id = get_button_id(node, adc_value);
    if (id == ADC_BTN_INVALID_ID) {
        if (act_id == ADC_BTN_INVALID_ACT_ID) {
            // No old act id and new act id.
            return ADC_BTN_STATE_IDLE;
        }
        if (btn_desc[act_id].click_cnt <= 1) {
            return ADC_BTN_STATE_IDLE;
        }
        // Have old act ID, new id is invalid
        // Need to send release event
        if (btn_desc[act_id].click_cnt < (info->press_judge_time / ADC_BTN_DETECT_TIME_MS)) {
            ESP_LOGD(TAG, "Pressed: Act ID:%d, ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
            st = ADC_BTN_STATE_RELEASE;
        } else {
            ESP_LOGD(TAG, "Long press release: Act ID:%d, ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
            st = ADC_BTN_STATE_LONG_RELEASE;
        }
        btn_desc[act_id].active_id = -1;
        btn_desc[act_id].long_click = 0;
        btn_desc[act_id].click_cnt = 0;
        return st;
    }
    // 1.ID is valid and act ID is invalid.
    if (act_id == ADC_BTN_INVALID_ACT_ID) {
        // First new act id
        btn_desc[id].active_id = id;
        return ADC_BTN_STATE_IDLE;
    }
    // 2.ID and act ID are valid, but not equal.
    if (id != act_id) {
        ESP_LOGW(TAG, "Old ID:%d, New ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
        // Invalid the act ID
        btn_desc[act_id].active_id = -1;
        btn_desc[act_id].long_click = 0;
        // Set the new id act ID
        btn_desc[id].active_id = id;
        // Maybe need to check release long pressed.
        if (btn_desc[act_id].click_cnt < ADC_BTN_DETECTED_CNT) {
            btn_desc[act_id].click_cnt = 0;
            return ADC_BTN_STATE_IDLE;
        }
        btn_desc[act_id].click_cnt = 0;
        // Have old act ID, new id is invalid
        // Need to send release event
        if (btn_desc[act_id].click_cnt < (info->press_judge_time / ADC_BTN_DETECT_TIME_MS)) {
            ESP_LOGD(TAG, "Pressed: Act ID:%d, ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
            return ADC_BTN_STATE_RELEASE;
        } else {
            ESP_LOGD(TAG, "Long press release: Act ID:%d, ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
            return ADC_BTN_STATE_LONG_RELEASE;
        }
    }
    // 3.ID and act ID are valid, and equal.
    btn_desc[act_id].click_cnt++;
    if (btn_desc[act_id].click_cnt == ADC_BTN_DETECTED_CNT) {
        return ADC_BTN_STATE_PRESSED;
    }

    if (btn_desc[act_id].long_click) {
        return ADC_BTN_STATE_IDLE;
    }
    if (btn_desc[act_id].click_cnt >= (info->press_judge_time / ADC_BTN_DETECT_TIME_MS)) {
        //Send long click event.
        ESP_LOGD(TAG, "Long press: Act ID:%d, ID:%d, Cnt:%d", act_id, id, btn_desc[act_id].click_cnt);
        st = ADC_BTN_STATE_LONG_PRESSED;
        btn_desc[act_id].long_click = 1;
    }
    return st;
}

static void button_task(void *parameters)
{
    s_task_flag = true;
    adc_btn_tag_t *tag = (adc_btn_tag_t *)parameters;
    adc_btn_list *head = tag->head;
    adc_btn_list *find = head;
    xEventGroupClearBits(s_event_bit, DESTROY_BIT);

#if ADC_LEGACY_API

#if CONFIG_IDF_TARGET_ESP32S2
    adc1_config_width(ADC_WIDTH_BIT_13);
#else
    adc1_config_width(ADC_WIDTH_BIT_12);
#endif  /* CONFIG_IDF_TARGET_ESP32S2 */

    while (find) {
        adc_arr_t *info = &(find->adc_info);
        reset_btn(find->btn_desc, info->total_steps);
        adc1_config_channel_atten(info->adc_ch, ADC_ATTEN_DEFAULT);
        find = find->next;
    }
    find = head;

#if defined ENABLE_ADC_VOLUME
    adc1_config_channel_atten(DIAL_adc_ch, ADC_ATTEN_DEFAULT);
#endif  /* ENABLE_ADC_VOLUME */

#else  /* ADC_LEGACY_API */

    adc_arr_t *info = &(head->adc_info);
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = info->adc_unit,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &s_adc_oneshot_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DEFAULT,
        .bitwidth = ADC_WIDTH_DEFAULT,
    };
    while (find) {
        adc_arr_t *info = &(find->adc_info);
        reset_btn(find->btn_desc, info->total_steps);
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_oneshot_handle, info->adc_ch, &config));
        find = find->next;
    }
    find = head;

#if defined ENABLE_ADC_VOLUME
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_oneshot_handle, DIAL_adc_ch, &config));
#endif  /* ENABLE_ADC_VOLUME */

    if (s_adc_cali_chan_handle == NULL) {
        adc_calibration_init(info->adc_unit, info->adc_ch, ADC_ATTEN_DEFAULT, &s_adc_cali_chan_handle);
    }

#endif  /* ADC_LEGACY_API */

#if defined ENABLE_ADC_VOLUME
    short adc_vol_prev = ADC_BTN_INVALID_ID;
    short adc_vol_cur = ADC_BTN_INVALID_ID;
    short internal_time_ms = DIAL_VOL_INTERVAL_TIME_MS / ADC_SAMPLE_INTERVAL_TIME_MS;  // 10 * 10 = 100ms
    static bool empty_flag;
    static bool full_flag;
    bool is_first_time = true;
#endif  /* ENABLE_ADC_VOLUME */

    static adc_btn_state_t cur_state = ADC_BTN_STATE_ADC;
    adc_btn_state_t btn_st = ADC_BTN_STATE_IDLE;
    int cur_act_id = ADC_BTN_INVALID_ACT_ID;
    while (s_task_flag) {
#if defined ENABLE_ADC_VOLUME
        if (internal_time_ms == 0) {
            adc_vol_cur = get_adc_voltage(info->adc_unit, DIAL_adc_ch);
            internal_time_ms = DIAL_VOL_INTERVAL_TIME_MS / ADC_SAMPLE_INTERVAL_TIME_MS;
            if (adc_vol_prev > 0) {
                short n = abs(adc_vol_cur - adc_vol_prev);
                if (is_first_time) {
                    is_first_time = false;
                }
                if (adc_vol_cur < ADC_VOL_EMPTY_THRESHOLD) {
                    if (empty_flag == false) {
                        ESP_LOGI(TAG, "ABS_LOW:%d, %d->0", n, adc_vol_cur / 25);
                        empty_flag = true;
                    }
                } else if (adc_vol_cur > ADC_VOL_FULL_THRESHOLD) {
                    if (full_flag == false) {
                        ESP_LOGI(TAG, "ABS_HIGH:%d, %d->100", n, adc_vol_cur / 25);
                        full_flag = true;
                    }
                } else if (n > ADC_VOL_DIFF_THRESHOLD) {
                    empty_flag = false;
                    full_flag = false;
                }
            }
            adc_vol_prev = adc_vol_cur;
        }
        internal_time_ms--;
#else
        find = head;
        while (find) {
            adc_arr_t *info = &(find->adc_info);
            int act_id = ADC_BTN_INVALID_ACT_ID;
            btn_decription *btn_desc = find->btn_desc;
            switch (cur_state) {
                case ADC_BTN_STATE_ADC: {
                    int adc = get_adc_voltage(info->adc_unit, info->adc_ch);
                    ESP_LOGD(TAG, "ADC Voltage: %d", adc);
                    for (int i = 0; i < info->total_steps; ++i) {
                        if (btn_desc[i].active_id > ADC_BTN_INVALID_ID) {
                            act_id = i;
                            break;
                        }
                    }
                    btn_st = get_adc_btn_state(adc, act_id, find);
                    if (btn_st != ADC_BTN_STATE_IDLE) {
                        cur_act_id = act_id;
                        cur_state = btn_st;
                        ESP_LOGD(TAG, "ADC ID:%d", act_id);
                    }
                    break;
                }
                case ADC_BTN_STATE_PRESSED: {
                    tag->btn_callback((void *)tag->user_data, info->adc_ch, cur_act_id, ADC_BTN_STATE_PRESSED);
                    cur_state = ADC_BTN_STATE_ADC;
                    break;
                }
                case ADC_BTN_STATE_LONG_PRESSED: {
                    tag->btn_callback((void *)tag->user_data, info->adc_ch, cur_act_id, ADC_BTN_STATE_LONG_PRESSED);
                    cur_state = ADC_BTN_STATE_ADC;
                    break;
                }
                case ADC_BTN_STATE_LONG_RELEASE: {
                    tag->btn_callback((void *)tag->user_data, info->adc_ch, cur_act_id, ADC_BTN_STATE_LONG_RELEASE);
                    cur_state = ADC_BTN_STATE_ADC;
                    break;
                }
                case ADC_BTN_STATE_RELEASE: {
                    tag->btn_callback((void *)tag->user_data, info->adc_ch, cur_act_id, ADC_BTN_STATE_RELEASE);
                    cur_state = ADC_BTN_STATE_ADC;
                    break;
                }
                default:
                    ESP_LOGE(TAG, "Not support state %d", cur_state);
                    break;
            }
            find = find->next;
        }
#endif  /* ENABLE_ADC_VOLUME */

        vTaskDelay(ADC_SAMPLE_INTERVAL_TIME_MS / portTICK_PERIOD_MS);
    }

#if !ADC_LEGACY_API
    ESP_ERROR_CHECK(adc_oneshot_del_unit(s_adc_oneshot_handle));
    if (s_adc_cali_chan_handle != NULL) {
        adc_calibration_deinit(s_adc_cali_chan_handle);
        s_adc_cali_chan_handle = NULL;
    }
#endif  /* !ADC_LEGACY_API */

    if (s_event_bit) {
        xEventGroupSetBits(s_event_bit, DESTROY_BIT);
    }
    audio_free(tag);
    vTaskDelete(NULL);
}

adc_btn_list *adc_btn_create_list(adc_arr_t *adc_conf, int channels)
{
    adc_btn_list *head = NULL;
    adc_btn_list *node = NULL;
    adc_btn_list *find = NULL;
    for (int i = 0; i < channels; i++) {
        node = (adc_btn_list *)audio_calloc(1, sizeof(adc_btn_list));
        if (NULL == node) {
            ESP_LOGE(TAG, "Memory allocation failed! Line: %d", __LINE__);
            goto create_list_fail;
        }
        memset(node, 0, sizeof(adc_btn_list));
        adc_arr_t *info = &(node->adc_info);
        memcpy(info, adc_conf + i, sizeof(adc_arr_t));
        int step_num = info->total_steps + 1;
        if (step_num > USER_KEY_MAX) {
            ESP_LOGW(TAG, "Step_num(%d) > USER_KEY_MAX(%d), clamp to USER_KEY_MAX", step_num, USER_KEY_MAX);
            step_num = USER_KEY_MAX;
        }
        info->adc_level_step = (int *)audio_calloc(1, step_num * sizeof(int));
        if (NULL == info->adc_level_step) {
            ESP_LOGE(TAG, "Memory allocation failed! Line: %d", __LINE__);
            audio_free(node);
            goto create_list_fail;
        }
        memset(info->adc_level_step, 0, step_num * sizeof(int));
        if (adc_conf[i].adc_level_step == NULL) {
            memcpy(info->adc_level_step, s_default_step_level, USER_KEY_MAX * sizeof(int));
        } else {
            memcpy(info->adc_level_step, adc_conf[i].adc_level_step, step_num * sizeof(int));
        }
        node->btn_desc = (btn_decription *)audio_calloc(1, sizeof(btn_decription) * (adc_conf[i].total_steps));
        if (NULL == node->btn_desc) {
            ESP_LOGE(TAG, "Memory allocation failed! Line: %d", __LINE__);
            audio_free(info->adc_level_step);
            audio_free(node);
            goto create_list_fail;
        }
        memset(node->btn_desc, 0, sizeof(btn_decription) * (adc_conf[i].total_steps));
        node->next = NULL;
        if (NULL == head) {
            head = node;
            find = head;
        } else {
            find->next = node;
            find = node;
        }
    }
    return head;

create_list_fail:
    adc_btn_destroy_list(head);
    return NULL;
}

esp_err_t adc_btn_destroy_list(adc_btn_list *head)
{
    if (head == NULL) {
        ESP_LOGD(TAG, "The head of list is null");
        return ESP_OK;
    }
    adc_btn_list *find = head;
    adc_btn_list *tmp = find;

    while (find) {
        adc_arr_t *info = &(find->adc_info);
        tmp = find->next;
        if (find->btn_desc) {
            audio_free(find->btn_desc);
        }
        if (info->adc_level_step) {
            audio_free(info->adc_level_step);
        }
        audio_free(find);
        find = tmp;
    }
    return ESP_OK;
}

void adc_btn_delete_task(void)
{
    if (s_task_flag) {
        s_task_flag = false;
    }

    if (s_event_bit) {
        xEventGroupWaitBits(s_event_bit, DESTROY_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        vEventGroupDelete(s_event_bit);
        s_event_bit = NULL;
    }
}

void adc_btn_init(void *user_data, adc_button_callback cb, adc_btn_list *head, adc_btn_task_cfg_t *task_cfg)
{
    adc_btn_tag_t *tag = audio_calloc(1, sizeof(adc_btn_tag_t));
    if (NULL == tag) {
        ESP_LOGE(TAG, "Memory allocation failed! Line: %d", __LINE__);
        return;
    }
    tag->user_data = user_data;
    tag->head = head;
    tag->btn_callback = cb;

    s_event_bit = xEventGroupCreate();

    if (audio_thread_create(&tag->audio_thread,
                        "button_task", button_task,
                        (void *)tag,
                        task_cfg->task_stack,
                        task_cfg->task_prio,
                        task_cfg->ext_stack,
                        task_cfg->task_core) != 0) {
        ESP_LOGE(TAG, "Create button_task failed!");
        audio_free(tag);
        vEventGroupDelete(s_event_bit);
        s_event_bit = NULL;
    }
}
