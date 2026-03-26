/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_codec_dev_defaults.h"
#include "esp_adc/adc_continuous.h"

#define TAG  "CODEC_ADC_IF"

/**
 * Compat shim for IDF < 5.5.2:
 *   - adc_continuous_parse_data() was added in IDF 5.5.2
 * Keep the parsed sample type private to this component so
 * backports of newer ADC helpers do not conflict with local
 * compatibility code on older branches.
 */
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 2)

#include "hal/adc_ll.h"

#ifdef ADC_LL_UNIT2_CHANNEL_SUBSTRATION
#define CODEC_ADC_UNIT2_CHANNEL_SUBSTRATION  ADC_LL_UNIT2_CHANNEL_SUBSTRATION
#else
#if CONFIG_IDF_TARGET_ESP32P4
#define CODEC_ADC_UNIT2_CHANNEL_SUBSTRATION  (2)
#else
#define CODEC_ADC_UNIT2_CHANNEL_SUBSTRATION  (0)
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */
#endif  /* ADC_LL_UNIT2_CHANNEL_SUBSTRATION */

#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 2) */

/**
 * Optional safety filter on read path:
 * - 0 (default): trust reconfigured pattern and output all parsed samples directly.
 * - 1: keep only samples matching current active pattern subset.
 */
#ifndef ADC_DATA_READ_FILTER_ENABLE
#define ADC_DATA_READ_FILTER_ENABLE  0
#endif  /* ADC_DATA_READ_FILTER_ENABLE */

typedef struct {
    adc_unit_t     unit;
    adc_channel_t  channel;
    uint32_t       raw_data;
    bool           valid;
} codec_adc_parsed_data_t;

typedef struct {
    audio_codec_data_if_t             base;
    adc_continuous_handle_t           handle;
    audio_codec_adc_continuous_cfg_t  cfg;  /* Immutable config snapshot from new() */
    /* base_patterns keeps the full configured channel/pattern table.
     * set_fmt() only selects a subset by index into this table. */
    adc_digi_pattern_config_t         base_patterns[SOC_ADC_PATT_LEN_MAX];
    uint8_t                           base_pattern_num;
    /* active_pattern_idx maps current runtime channels -> base_patterns index. */
    uint8_t                           active_pattern_idx[SOC_ADC_PATT_LEN_MAX];
    uint8_t                           active_pattern_num;
    uint8_t                           out_bits;  /* Output PCM bits (16 or 32) */
    bool                              use_ext_handle;
    bool                              is_open;
    bool                              enabled;
    uint8_t                          *raw_read_buf;
    size_t                            raw_read_buf_size;
    codec_adc_parsed_data_t          *parsed_buf;
    uint32_t                          current_sample_rate_hz;
    uint8_t                           base_pattern_bits[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];
    uint8_t                           active_pattern_bits[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];
} adc_data_t;

static inline bool adc_data_bit_width_valid(uint8_t bit_width)
{
    return bit_width == ADC_BITWIDTH_DEFAULT ||
           (bit_width >= ADC_BITWIDTH_9 && bit_width <= SOC_ADC_DIGI_MAX_BITWIDTH);
}

static inline bool adc_unit_supported(adc_unit_t unit)
{
#ifdef SOC_ADC_DIG_SUPPORTED_UNIT
    return SOC_ADC_DIG_SUPPORTED_UNIT(unit);
#else
    return unit == ADC_UNIT_1;
#endif  /* SOC_ADC_DIG_SUPPORTED_UNIT */
}

static inline uint8_t adc_get_effective_sample_bits(uint8_t bit_width)
{
    return bit_width == ADC_BITWIDTH_DEFAULT ? SOC_ADC_DIGI_MAX_BITWIDTH : bit_width;
}

static inline int32_t adc_centered_to_pcm(int32_t centered, uint8_t out_bits, uint8_t sample_bits)
{
    return (int32_t)(centered * (int64_t)(1ULL << (out_bits - sample_bits)));
}

static inline bool adc_unit_channel_in_range(adc_unit_t unit, adc_channel_t channel)
{
    return unit < SOC_ADC_PERIPH_NUM && channel < SOC_ADC_MAX_CHANNEL_NUM;
}

static inline void adc_store_pattern_bits(uint8_t pattern_bits[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM],
                                          adc_unit_t unit, adc_channel_t channel, uint8_t sample_bits)
{
    if (adc_unit_channel_in_range(unit, channel) && pattern_bits[unit][channel] == 0) {
        pattern_bits[unit][channel] = sample_bits;
    }
}

static uint8_t adc_get_sample_bits_by_unit_channel(const adc_data_t *adc_data, adc_unit_t unit, adc_channel_t channel)
{
    if (adc_unit_channel_in_range(unit, channel)) {
        if (adc_data->active_pattern_bits[unit][channel]) {
            return adc_data->active_pattern_bits[unit][channel];
        }
        if (adc_data->base_pattern_bits[unit][channel]) {
            return adc_data->base_pattern_bits[unit][channel];
        }
    }
    return SOC_ADC_DIGI_MAX_BITWIDTH;
}

#if ADC_DATA_READ_FILTER_ENABLE
static bool adc_sample_allowed(const adc_data_t *adc_data, adc_unit_t unit, adc_channel_t channel)
{
    return adc_unit_channel_in_range(unit, channel) && adc_data->active_pattern_bits[unit][channel] != 0;
}
#endif  /* ADC_DATA_READ_FILTER_ENABLE */

/**
 * Parse raw ADC DMA bytes into component-private parsed sample array.
 * On IDF >= 5.5.2 delegates to the real driver API;
 * on older IDF performs the parsing locally with full access to
 * adc_data_t (format, active patterns, etc.).
 */
static esp_err_t adc_parse_raw_data(adc_data_t *adc_data,
                                    const uint8_t *raw_data,
                                    uint32_t raw_data_size,
                                    codec_adc_parsed_data_t *parsed_data,
                                    uint32_t *num_parsed_samples)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2)
    if (!raw_data || !parsed_data || !num_parsed_samples) {
        return ESP_ERR_INVALID_ARG;
    }
    if (raw_data_size == 0 || raw_data_size % SOC_ADC_DIGI_RESULT_BYTES != 0) {
        *num_parsed_samples = 0;
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t sample_count = raw_data_size / SOC_ADC_DIGI_RESULT_BYTES;
    adc_continuous_data_t sdk_parsed_data[sample_count];
    esp_err_t ret = adc_continuous_parse_data(adc_data->handle, raw_data, raw_data_size,
                                              sdk_parsed_data, num_parsed_samples);
    if (ret != ESP_OK) {
        return ret;
    }
    for (uint32_t i = 0; i < *num_parsed_samples; i++) {
        parsed_data[i].unit = sdk_parsed_data[i].unit;
        parsed_data[i].channel = sdk_parsed_data[i].channel;
        parsed_data[i].raw_data = sdk_parsed_data[i].raw_data;
        parsed_data[i].valid = sdk_parsed_data[i].valid;
    }
    return ESP_OK;
#else
    if (!raw_data || !parsed_data || !num_parsed_samples) {
        return ESP_ERR_INVALID_ARG;
    }
    if (raw_data_size == 0 || raw_data_size % SOC_ADC_DIGI_RESULT_BYTES != 0) {
        *num_parsed_samples = 0;
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t samples = raw_data_size / SOC_ADC_DIGI_RESULT_BYTES;
    for (uint32_t i = 0; i < samples; i++) {
        adc_digi_output_data_t *p =
            (adc_digi_output_data_t *)&raw_data[i * SOC_ADC_DIGI_RESULT_BYTES];
#if CONFIG_IDF_TARGET_ESP32
        parsed_data[i].unit     = ADC_UNIT_1;
        parsed_data[i].channel  = p->type1.channel;
        parsed_data[i].raw_data = p->type1.data;
#elif CONFIG_IDF_TARGET_ESP32S2
        if (adc_data->cfg.format == ADC_DIGI_OUTPUT_FORMAT_TYPE2) {
            parsed_data[i].unit     = p->type2.unit ? ADC_UNIT_2 : ADC_UNIT_1;
            parsed_data[i].channel  = p->type2.channel;
            parsed_data[i].raw_data = p->type2.data;
        } else {
            bool uses_adc1 = false;
            for (int j = 0; j < adc_data->active_pattern_num; j++) {
                if ((adc_unit_t)adc_data->base_patterns[adc_data->active_pattern_idx[j]].unit == ADC_UNIT_1) {
                    uses_adc1 = true;
                    break;
                }
            }
            parsed_data[i].unit     = uses_adc1 ? ADC_UNIT_1 : ADC_UNIT_2;
            parsed_data[i].channel  = p->type1.channel;
            parsed_data[i].raw_data = p->type1.data;
        }
#else
#if SOC_ADC_PERIPH_NUM == 1
        parsed_data[i].unit = ADC_UNIT_1;
#else
        parsed_data[i].unit = p->type2.unit ? ADC_UNIT_2 : ADC_UNIT_1;
#endif  /* SOC_ADC_PERIPH_NUM == 1 */
        parsed_data[i].channel  = (parsed_data[i].unit == ADC_UNIT_2)
                                      ? p->type2.channel - CODEC_ADC_UNIT2_CHANNEL_SUBSTRATION
                                      : p->type2.channel;
        parsed_data[i].raw_data = p->type2.data;
#endif  /* CONFIG_IDF_TARGET_ESP32 */
        parsed_data[i].valid =
            (parsed_data[i].channel < SOC_ADC_CHANNEL_NUM(parsed_data[i].unit));
    }
    *num_parsed_samples = samples;
    return ESP_OK;
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2) */
}

static adc_digi_convert_mode_t adc_calc_conv_mode(const adc_digi_pattern_config_t *patterns, uint8_t pattern_num)
{
    bool has_unit1 = false;
    bool has_unit2 = false;
    for (int i = 0; i < pattern_num; i++) {
        if ((adc_unit_t)patterns[i].unit == ADC_UNIT_1) {
            has_unit1 = true;
        } else if ((adc_unit_t)patterns[i].unit == ADC_UNIT_2) {
            has_unit2 = true;
        }
    }
    if (has_unit1 && has_unit2) {
        return ADC_CONV_BOTH_UNIT;
    }
    return has_unit2 ? ADC_CONV_SINGLE_UNIT_2 : ADC_CONV_SINGLE_UNIT_1;
}

static esp_err_t adc_apply_patterns(adc_data_t *adc_data, const uint8_t *pattern_idx, uint8_t pattern_num, uint32_t sample_rate_hz)
{
    ESP_RETURN_ON_FALSE(pattern_num > 0, ESP_ERR_INVALID_ARG, TAG, "Pattern number should be > 0");
    ESP_RETURN_ON_FALSE(sample_rate_hz >= (SOC_ADC_SAMPLE_FREQ_THRES_LOW + pattern_num - 1) / pattern_num,
                        ESP_ERR_INVALID_ARG, TAG,
                        "Sample rate should be >= %lu", (unsigned long)((SOC_ADC_SAMPLE_FREQ_THRES_LOW + pattern_num - 1) / pattern_num));
    ESP_RETURN_ON_FALSE(sample_rate_hz <= (SOC_ADC_SAMPLE_FREQ_THRES_HIGH / pattern_num), ESP_ERR_INVALID_ARG, TAG,
                        "Sample rate out of range: %lu * %u", (unsigned long)sample_rate_hz, pattern_num);

    /* Build a temporary hardware pattern list from selected base indices. */
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    uint8_t active_pattern_bits[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM] = {0};
    for (int i = 0; i < pattern_num; i++) {
        adc_pattern[i] = adc_data->base_patterns[pattern_idx[i]];
        adc_store_pattern_bits(active_pattern_bits,
                               (adc_unit_t)adc_pattern[i].unit,
                               (adc_channel_t)adc_pattern[i].channel,
                               adc_get_effective_sample_bits(adc_pattern[i].bit_width));
    }

    /* IDF continuous driver expects total conversion rate, so multiply by channel count. */
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = sample_rate_hz * pattern_num,
        .conv_mode = adc_calc_conv_mode(adc_pattern, pattern_num),
        .format = adc_data->cfg.format,
        .pattern_num = pattern_num,
        .adc_pattern = adc_pattern,
    };

    esp_err_t ret = adc_continuous_config(adc_data->handle, &dig_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Call to adc_continuous_config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    memcpy(adc_data->active_pattern_idx, pattern_idx, pattern_num);
    adc_data->active_pattern_num = pattern_num;
    memcpy(adc_data->active_pattern_bits, active_pattern_bits, sizeof(adc_data->active_pattern_bits));
    return ESP_OK;
}

static bool _adc_data_is_open(const audio_codec_data_if_t *h)
{
    adc_data_t *adc_data = (adc_data_t *)h;
    return adc_data ? adc_data->is_open : false;
}

static int _adc_data_enable(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, bool enable)
{
    adc_data_t *adc_data = (adc_data_t *)h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "ADC data is NULL");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "ADC data not open");
    ESP_RETURN_ON_FALSE(dev_type == ESP_CODEC_DEV_TYPE_IN, ESP_CODEC_DEV_INVALID_ARG, TAG, "Invalid dev_type");

    if (enable == adc_data->enabled) {
        return ESP_CODEC_DEV_OK;
    }

    esp_err_t ret = ESP_OK;
    if (enable) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        /* Drop stale frames before (re)start to keep first read aligned with current config. */
        ret = adc_continuous_flush_pool(adc_data->handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_flush_pool failed before start: %s", esp_err_to_name(ret));
            return ESP_CODEC_DEV_DRV_ERR;
        }
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0) */
        ret = adc_continuous_start(adc_data->handle);
        if (ret == ESP_OK) {
            adc_data->enabled = true;
        } else {
            ESP_LOGE(TAG, "Call to adc_continuous_start failed: %s", esp_err_to_name(ret));
        }
    } else {
        ret = adc_continuous_stop(adc_data->handle);
        if (ret == ESP_OK) {
            adc_data->enabled = false;
        } else {
            ESP_LOGE(TAG, "Call to adc_continuous_stop failed: %s", esp_err_to_name(ret));
        }
    }
    return ret == ESP_OK ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static int _adc_data_read(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    adc_data_t *adc_data = (adc_data_t *)h;
    ESP_RETURN_ON_FALSE(adc_data != NULL && data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "ADC data not open");
    ESP_RETURN_ON_FALSE(adc_data->enabled, ESP_CODEC_DEV_WRONG_STATE, TAG, "ADC data not enabled");
    ESP_RETURN_ON_FALSE(adc_data->out_bits == 16 || adc_data->out_bits == 32,
                        ESP_CODEC_DEV_WRONG_STATE, TAG, "Invalid output bits");
    ESP_RETURN_ON_FALSE(size >= 0, ESP_CODEC_DEV_INVALID_ARG, TAG, "Read size should be >= 0");
    if (size == 0) {
        return ESP_CODEC_DEV_OK;
    }

    const int out_bytes_per_sample = adc_data->out_bits / 8;
    ESP_RETURN_ON_FALSE((size % out_bytes_per_sample) == 0,
                        ESP_CODEC_DEV_INVALID_ARG, TAG, "Read size should be %d-byte aligned", out_bytes_per_sample);

    uint32_t ret_num = 0;
    int target_samples = size / out_bytes_per_sample;
    int produced = 0;
    uint8_t *dst = data;

    while (produced < target_samples) {
        int samples_left = target_samples - produced;
        size_t request = samples_left * SOC_ADC_DIGI_RESULT_BYTES;
        if (request > adc_data->raw_read_buf_size) {
            request = adc_data->raw_read_buf_size;
        }
        if (request < SOC_ADC_DIGI_RESULT_BYTES) {
            request = SOC_ADC_DIGI_RESULT_BYTES;
        }

        /* 1) Pull raw DMA bytes from ADC continuous driver. */
        esp_err_t ret = adc_continuous_read(adc_data->handle, adc_data->raw_read_buf, request, &ret_num, 1000 / portTICK_PERIOD_MS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_read failed: %s", esp_err_to_name(ret));
            return ESP_CODEC_DEV_DRV_ERR;
        }
        if (ret_num < SOC_ADC_DIGI_RESULT_BYTES) {
            ESP_LOGE(TAG, "Call to adc_continuous_read returned too few bytes: %lu", (unsigned long)ret_num);
            return ESP_CODEC_DEV_DRV_ERR;
        }

        /* 2) Parse raw bytes into (unit/channel/raw_data/valid) tuples. */
        uint32_t num_parsed = 0;
        ret = adc_parse_raw_data(adc_data, adc_data->raw_read_buf, ret_num,
                                 adc_data->parsed_buf, &num_parsed);
        if (ret != ESP_OK || num_parsed == 0) {
            ESP_LOGE(TAG, "Call to adc_parse_raw_data failed: %s, parsed:%lu",
                     esp_err_to_name(ret), (unsigned long)num_parsed);
            return ESP_CODEC_DEV_DRV_ERR;
        }

        int valid = 0;
        for (uint32_t i = 0; i < num_parsed; i++) {
            const codec_adc_parsed_data_t *s = &adc_data->parsed_buf[i];
            if (!s->valid) {
                continue;
            }
#if ADC_DATA_READ_FILTER_ENABLE
            if (!adc_sample_allowed(adc_data, s->unit, s->channel)) {
                continue;
            }
#endif  /* ADC_DATA_READ_FILTER_ENABLE */
            /* 3) Convert each parsed sample to requested PCM width. */
            uint8_t sample_bits = adc_get_sample_bits_by_unit_channel(adc_data, s->unit, s->channel);
            ESP_RETURN_ON_FALSE(sample_bits > 0 && sample_bits <= adc_data->out_bits && sample_bits <= 31,
                                ESP_CODEC_DEV_DRV_ERR, TAG, "Invalid sample bits: %u", sample_bits);
            uint32_t sample_mask = (uint32_t)((1ULL << sample_bits) - 1U);
            int32_t raw_centered = (int32_t)(s->raw_data & sample_mask) - (int32_t)(1U << (sample_bits - 1));
            if (adc_data->out_bits == 16) {
                int16_t sample_16 = (int16_t)adc_centered_to_pcm(raw_centered, 16, sample_bits);
                memcpy(dst + (produced + valid) * out_bytes_per_sample, &sample_16, sizeof(sample_16));
            } else {
                int32_t sample_32 = adc_centered_to_pcm(raw_centered, 32, sample_bits);
                memcpy(dst + (produced + valid) * out_bytes_per_sample, &sample_32, sizeof(sample_32));
            }
            valid++;
            if (produced + valid >= target_samples) {
                break;
            }
        }
        ESP_RETURN_ON_FALSE(valid > 0, ESP_CODEC_DEV_DRV_ERR, TAG, "No valid ADC samples in frame");
        produced += valid;
    }
    return ESP_CODEC_DEV_OK;
}

static int _adc_data_set_fmt(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, esp_codec_dev_sample_info_t *fs)
{
    adc_data_t *adc_data = (adc_data_t *)h;
    ESP_RETURN_ON_FALSE(adc_data != NULL && fs != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "ADC data not open");
    ESP_RETURN_ON_FALSE(dev_type == ESP_CODEC_DEV_TYPE_IN, ESP_CODEC_DEV_INVALID_ARG, TAG, "Invalid dev_type");
    ESP_RETURN_ON_FALSE(fs->bits_per_sample == 16 || fs->bits_per_sample == 32,
                        ESP_CODEC_DEV_INVALID_ARG, TAG, "Only support 16-bit or 32-bit");
    ESP_RETURN_ON_FALSE(fs->sample_rate > 0, ESP_CODEC_DEV_INVALID_ARG, TAG, "Sample_rate should be > 0");

    uint8_t active_idx[SOC_ADC_PATT_LEN_MAX] = {0};
    uint8_t active_num = 0;
    uint8_t old_active_idx[SOC_ADC_PATT_LEN_MAX] = {0};
    uint8_t old_active_num = adc_data->active_pattern_num;
    uint8_t old_out_bits = adc_data->out_bits;
    uint32_t old_sample_rate_hz = adc_data->current_sample_rate_hz;
    memcpy(old_active_idx, adc_data->active_pattern_idx, old_active_num);

    if (fs->channel_mask == 0) {
        /* No mask: use first N channels from base order, where N == fs->channel. */
        ESP_RETURN_ON_FALSE(fs->channel > 0 && fs->channel <= adc_data->base_pattern_num,
                            ESP_CODEC_DEV_INVALID_ARG, TAG, "Channel out of range");
        active_num = fs->channel;
        for (int i = 0; i < active_num; i++) {
            active_idx[i] = i;
        }
        if (fs->channel != adc_data->base_pattern_num) {
            ESP_LOGW(TAG, "Channel(%u) mismatch with configured channels(%u) while mask=0, use first %u channels",
                     fs->channel, adc_data->base_pattern_num, fs->channel);
        }
    } else {
        /* Mask mode: bit i maps to base_patterns[i]. */
        if ((fs->channel_mask >> adc_data->base_pattern_num) != 0) {
            ESP_LOGE(TAG, "Channel_mask out of range");
            return ESP_CODEC_DEV_INVALID_ARG;
        }
        for (uint8_t i = 0; i < adc_data->base_pattern_num; i++) {
            if (fs->channel_mask & (1U << i)) {
                active_idx[active_num++] = i;
            }
        }
        ESP_RETURN_ON_FALSE(active_num > 0, ESP_CODEC_DEV_INVALID_ARG, TAG, "Empty channel_mask");
        ESP_RETURN_ON_FALSE(fs->channel == active_num, ESP_CODEC_DEV_INVALID_ARG, TAG, "Channel mismatch");
    }

    /* Reconfigure sequence: stop -> config -> flush -> optional restart. */
    bool was_enabled = adc_data->enabled;
    if (was_enabled) {
        esp_err_t stop_ret = adc_continuous_stop(adc_data->handle);
        if (stop_ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_stop failed before set_fmt reconfig: %s", esp_err_to_name(stop_ret));
            return ESP_CODEC_DEV_DRV_ERR;
        }
        adc_data->enabled = false;
    }

    esp_err_t ret = adc_apply_patterns(adc_data, active_idx, active_num, fs->sample_rate);
    if (ret == ESP_OK) {
        adc_data->out_bits = fs->bits_per_sample;
        adc_data->current_sample_rate_hz = fs->sample_rate;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    if (ret == ESP_OK) {
        /* Remove old-format frames that may remain in internal pool after reconfig. */
        esp_err_t flush_ret = adc_continuous_flush_pool(adc_data->handle);
        if (flush_ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_flush_pool failed after set_fmt: %s", esp_err_to_name(flush_ret));
            ret = flush_ret;
        }
    }
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0) */

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Set_fmt failed (%s), fallback to previous configuration",
                 esp_err_to_name(ret));
        esp_err_t rollback_ret = adc_apply_patterns(adc_data, old_active_idx, old_active_num, old_sample_rate_hz);
        if (rollback_ret == ESP_OK) {
            adc_data->out_bits = old_out_bits;
            adc_data->current_sample_rate_hz = old_sample_rate_hz;
            ESP_LOGW(TAG, "Fallback done: keep previous ADC format and channel settings");
        } else {
            ESP_LOGE(TAG, "Fallback to previous configuration failed (%s)", esp_err_to_name(rollback_ret));
        }
    }

    if (was_enabled) {
        esp_err_t start_ret = adc_continuous_start(adc_data->handle);
        if (start_ret == ESP_OK) {
            adc_data->enabled = true;
        } else {
            ESP_LOGE(TAG, "Call to adc_continuous_start failed after set_fmt: %s", esp_err_to_name(start_ret));
            ret = start_ret;
        }
    }
    return ret == ESP_OK ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static int _adc_data_close(const audio_codec_data_if_t *h)
{
    adc_data_t *adc_data = (adc_data_t *)h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "ADC data is NULL");

    if (adc_data->enabled) {
        esp_err_t stop_ret = adc_continuous_stop(adc_data->handle);
        if (stop_ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_stop failed during close: %s", esp_err_to_name(stop_ret));
        } else {
            adc_data->enabled = false;
        }
    }
    /* Only destroy ADC handle when it was created locally by this data_if. */
    if (!adc_data->use_ext_handle) {
        esp_err_t deinit_ret = adc_continuous_deinit(adc_data->handle);
        if (deinit_ret != ESP_OK) {
            ESP_LOGE(TAG, "Call to adc_continuous_deinit failed during close: %s", esp_err_to_name(deinit_ret));
        }
    }
    if (adc_data->parsed_buf) {
        free(adc_data->parsed_buf);
        adc_data->parsed_buf = NULL;
    }
    if (adc_data->raw_read_buf) {
        free(adc_data->raw_read_buf);
        adc_data->raw_read_buf = NULL;
    }
    adc_data->is_open = false;
    return ESP_CODEC_DEV_OK;
}

const audio_codec_data_if_t *audio_codec_new_adc_data(audio_codec_adc_cfg_t *adc_cfg)
{
    ESP_RETURN_ON_FALSE(adc_cfg != NULL, NULL, TAG, "ADC cfg is NULL");
    ESP_RETURN_ON_FALSE(adc_cfg->continuous_cfg.pattern_num > 0, NULL, TAG, "Pattern_num should be > 0");
    ESP_RETURN_ON_FALSE(adc_cfg->continuous_cfg.pattern_num <= SOC_ADC_PATT_LEN_MAX, NULL, TAG, "Pattern_num too large");
    ESP_RETURN_ON_FALSE(adc_cfg->continuous_cfg.conv_frame_size > 0, NULL, TAG, "Conv_frame_size should be > 0");
    ESP_RETURN_ON_FALSE(adc_cfg->continuous_cfg.sample_freq_hz > 0, NULL, TAG, "Sample_freq_hz should be > 0");
    ESP_RETURN_ON_FALSE(adc_cfg->continuous_cfg.cfg_mode == AUDIO_CODEC_ADC_CFG_MODE_SINGLE_UNIT || adc_cfg->continuous_cfg.cfg_mode == AUDIO_CODEC_ADC_CFG_MODE_PATTERN,
                        NULL, TAG, "Invalid cfg_mode");

    /* One allocation owns all runtime state; deleted by audio_codec_delete_data_if(). */
    adc_data_t *adc_data = calloc(1, sizeof(adc_data_t));
    ESP_RETURN_ON_FALSE(adc_data != NULL, NULL, TAG, "Allocate adc_data failed");

    adc_data->cfg = adc_cfg->continuous_cfg;
    adc_data->base_pattern_num = adc_cfg->continuous_cfg.pattern_num;
    adc_data->out_bits = 16;
    adc_data->current_sample_rate_hz = adc_cfg->continuous_cfg.sample_freq_hz;
    memset(adc_data->base_pattern_bits, 0, sizeof(adc_data->base_pattern_bits));
    memset(adc_data->active_pattern_bits, 0, sizeof(adc_data->active_pattern_bits));
    adc_data->raw_read_buf_size = adc_data->cfg.conv_frame_size;
    if (adc_data->raw_read_buf_size < SOC_ADC_DIGI_RESULT_BYTES) {
        adc_data->raw_read_buf_size = SOC_ADC_DIGI_RESULT_BYTES;
    }

    adc_data->raw_read_buf_size = (adc_data->raw_read_buf_size / SOC_ADC_DIGI_RESULT_BYTES) * SOC_ADC_DIGI_RESULT_BYTES;

    if (adc_data->cfg.cfg_mode == AUDIO_CODEC_ADC_CFG_MODE_SINGLE_UNIT) {
        /* Expand legacy single_unit config to uniform pattern table. */
        uint8_t sample_bits = adc_get_effective_sample_bits(adc_data->cfg.cfg.single_unit.bit_width);
        if (!adc_data_bit_width_valid(sample_bits)) {
            ESP_LOGE(TAG, "Invalid bit_width");
            goto new_err;
        }
        for (int i = 0; i < adc_data->base_pattern_num; i++) {
            if (!adc_unit_supported((adc_unit_t)adc_data->cfg.cfg.single_unit.unit_id)) {
                ESP_LOGE(TAG, "ADC unit %u not supported in continuous mode", adc_data->cfg.cfg.single_unit.unit_id);
                goto new_err;
            }
            if (adc_data->cfg.cfg.single_unit.channel_id[i] >= SOC_ADC_CHANNEL_NUM((adc_unit_t)adc_data->cfg.cfg.single_unit.unit_id)) {
                ESP_LOGE(TAG, "Invalid ADC channel %u for unit %u", adc_data->cfg.cfg.single_unit.channel_id[i],
                         adc_data->cfg.cfg.single_unit.unit_id);
                goto new_err;
            }
            adc_data->base_patterns[i].unit = adc_data->cfg.cfg.single_unit.unit_id;
            adc_data->base_patterns[i].atten = adc_data->cfg.cfg.single_unit.atten;
            adc_data->base_patterns[i].bit_width = adc_data->cfg.cfg.single_unit.bit_width;
            adc_data->base_patterns[i].channel = adc_data->cfg.cfg.single_unit.channel_id[i];
            adc_store_pattern_bits(adc_data->base_pattern_bits,
                                   (adc_unit_t)adc_data->base_patterns[i].unit,
                                   (adc_channel_t)adc_data->base_patterns[i].channel,
                                   sample_bits);
        }
    } else {
        /* Pattern mode: use caller-provided unit/channel entries directly. */
        memcpy(adc_data->base_patterns, adc_data->cfg.cfg.patterns,
               adc_data->base_pattern_num * sizeof(adc_digi_pattern_config_t));
        for (int i = 0; i < adc_data->base_pattern_num; i++) {
            if (!adc_data_bit_width_valid(adc_data->base_patterns[i].bit_width)) {
                ESP_LOGE(TAG, "Invalid bit_width in patterns[%d]", i);
                goto new_err;
            }
            if (!adc_unit_supported((adc_unit_t)adc_data->base_patterns[i].unit)) {
                ESP_LOGE(TAG, "ADC unit %u not supported in continuous mode", adc_data->base_patterns[i].unit);
                goto new_err;
            }
            if (adc_data->base_patterns[i].channel >= SOC_ADC_CHANNEL_NUM((adc_unit_t)adc_data->base_patterns[i].unit)) {
                ESP_LOGE(TAG, "Invalid ADC channel %u for unit %u", adc_data->base_patterns[i].channel,
                         adc_data->base_patterns[i].unit);
                goto new_err;
            }
            adc_store_pattern_bits(adc_data->base_pattern_bits,
                                   (adc_unit_t)adc_data->base_patterns[i].unit,
                                   (adc_channel_t)adc_data->base_patterns[i].channel,
                                   adc_get_effective_sample_bits(adc_data->base_patterns[i].bit_width));
        }
    }

    adc_data->raw_read_buf = calloc(1, adc_data->raw_read_buf_size);
    if (adc_data->raw_read_buf == NULL) {
        free(adc_data);
        return NULL;
    }

    uint32_t parsed_buf_count = adc_data->raw_read_buf_size / SOC_ADC_DIGI_RESULT_BYTES;
    if (parsed_buf_count == 0) {
        parsed_buf_count = 1;
    }
    adc_data->parsed_buf = calloc(parsed_buf_count, sizeof(codec_adc_parsed_data_t));
    if (adc_data->parsed_buf == NULL) {
        free(adc_data->raw_read_buf);
        free(adc_data);
        return NULL;
    }

    if (adc_cfg->handle == NULL) {
        /* Local handle mode: lifecycle owned by this module. */
        adc_continuous_handle_cfg_t adc_handle_cfg = {
            .max_store_buf_size = adc_data->cfg.max_store_buf_size,
            .conv_frame_size = adc_data->cfg.conv_frame_size,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
            .flags.flush_pool = true,
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0) */
        };
        if (adc_continuous_new_handle(&adc_handle_cfg, &adc_data->handle) != ESP_OK) {
            goto new_err;
        }
        adc_data->use_ext_handle = false;
    } else {
        /* Reuse mode: external owner manages deinit, we only start/stop/config. */
        adc_data->handle = (adc_continuous_handle_t)adc_cfg->handle;
        adc_data->use_ext_handle = true;
    }

    for (int i = 0; i < adc_data->base_pattern_num; i++) {
        adc_data->active_pattern_idx[i] = i;
    }
    if (adc_apply_patterns(adc_data, adc_data->active_pattern_idx, adc_data->base_pattern_num,
                           adc_data->cfg.sample_freq_hz) != ESP_OK) {
        goto new_err;
    }

    adc_data->base.open = NULL;
    adc_data->base.is_open = _adc_data_is_open;
    adc_data->base.enable = _adc_data_enable;
    adc_data->base.read = _adc_data_read;
    adc_data->base.write = NULL;
    adc_data->base.set_fmt = _adc_data_set_fmt;
    adc_data->base.close = _adc_data_close;
    adc_data->is_open = true;
    return &adc_data->base;

new_err:
    /* Centralized failure cleanup for all partially-initialized resources. */
    if (adc_data->handle && !adc_data->use_ext_handle) {
        adc_continuous_deinit(adc_data->handle);
    }
    if (adc_data->parsed_buf) {
        free(adc_data->parsed_buf);
    }
    if (adc_data->raw_read_buf) {
        free(adc_data->raw_read_buf);
    }
    free(adc_data);
    return NULL;
}
