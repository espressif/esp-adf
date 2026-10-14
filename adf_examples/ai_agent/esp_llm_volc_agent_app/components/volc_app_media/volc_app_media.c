/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <inttypes.h>
#include <stdlib.h>

#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_extractor_defaults.h"
#include "esp_log.h"
#include "esp_media_service.h"
#include "esp_player_service_playback.h"
#include "esp_service_scheduler.h"

#include "volc_app_media.h"

#define VOLC_APP_MEDIA_STREAM              ESP_MEDIA_DEFAULT_STREAM
#define VOLC_APP_MEDIA_PLAYER_STREAM_NUM   1
#define VOLC_APP_MEDIA_AENC_TASK_STACK     (40 * 1024)
#define VOLC_APP_MEDIA_AFE_FEED_STACK      (8 * 1024)
#define VOLC_APP_MEDIA_AFE_FETCH_STACK     (12 * 1024)
#define VOLC_APP_MEDIA_AFE_FEED_CORE       (1)
#define VOLC_APP_MEDIA_AFE_FEED_PRIO       (15)
#define VOLC_APP_MEDIA_AFE_FETCH_CORE      (0)
#define VOLC_APP_MEDIA_AFE_FETCH_PRIO      (16)
#define VOLC_APP_MEDIA_AENC_CORE           (1)
#define VOLC_APP_MEDIA_AENC_PRIO           (5)
#define VOLC_APP_MEDIA_DEVICE_SAMPLE_RATE  (16000U)
#define VOLC_APP_MEDIA_ADC_MIC_GAIN_DB     (32.0f)
#define VOLC_APP_MEDIA_ADC_REF_GAIN_DB     (20.0f)
#define VOLC_APP_MEDIA_DAC_CHANNELS        (1)
#define VOLC_APP_MEDIA_ADC_CH_FALLBACK     (2)
#define VOLC_APP_MEDIA_CODEC_BITS          (16)

static const char *TAG = "VOLC_APP_MEDIA";

/**
 * @brief  Private application media graph
 */
struct volc_app_media {
    esp_capture_service_t  *audio_capture;  /*!< Application-owned audio capture */
    esp_player_service_t   *audio_player;   /*!< Application-owned audio player */
    esp_llm_service_t      *llm;            /*!< Borrowed LLM while the graph is linked */
};

static void record_first_error(esp_err_t *result, esp_err_t error)
{
    if (result != NULL && *result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

static void try_stop(void *svc, const char *name, esp_err_t *result)
{
    if (svc == NULL) {
        return;
    }
    esp_err_t ret = esp_service_stop(ESP_SERVICE_BASE(svc));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Stop %s failed: %s", name, esp_err_to_name(ret));
        record_first_error(result, ret);
    }
}

static esp_err_t capture_scheduler_cb(const esp_service_thread_request_t *request,
                                      esp_service_thread_cfg_t *cfg,
                                      void *ctx)
{
    (void)ctx;
    if (request == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Capture scheduler failed: request or cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FEED)) {
        cfg->stack_size = VOLC_APP_MEDIA_AFE_FEED_STACK;
        cfg->core_id = VOLC_APP_MEDIA_AFE_FEED_CORE;
        cfg->priority = VOLC_APP_MEDIA_AFE_FEED_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH)) {
        cfg->stack_size = VOLC_APP_MEDIA_AFE_FETCH_STACK;
        cfg->core_id = VOLC_APP_MEDIA_AFE_FETCH_CORE;
        cfg->priority = VOLC_APP_MEDIA_AFE_FETCH_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_0) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_1)) {
        cfg->stack_size = VOLC_APP_MEDIA_AENC_TASK_STACK;
        cfg->is_ext = true;
        cfg->core_id = VOLC_APP_MEDIA_AENC_CORE;
        cfg->priority = VOLC_APP_MEDIA_AENC_PRIO;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t resolve_stream_info(const esp_llm_volc_cfg_t *volc_cfg,
                                     esp_llm_media_stream_t stream,
                                     esp_media_track_info_t *out_info)
{
    if (volc_cfg == NULL || out_info == NULL || stream >= ESP_LLM_MEDIA_STREAM_COUNT) {
        ESP_LOGE(TAG, "Resolve stream failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_cfg->service.streams[stream].enabled) {
        ESP_LOGE(TAG, "Resolve stream failed: stream %d is disabled", (int)stream);
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = volc_cfg->service.streams[stream].info;
    return ESP_OK;
}

static esp_codec_dev_handle_t codec_dev(const char *name)
{
    dev_audio_codec_handles_t *dev = NULL;
    if (esp_board_manager_get_device_handle(name, (void **)&dev) != ESP_OK || dev == NULL) {
        return NULL;
    }
    return dev->codec_dev;
}

static uint8_t channel_count_from_label(const char *label)
{
    if (label == NULL || label[0] == '\0') {
        return 0;
    }
    uint8_t n = 1;
    for (const char *p = label; *p != '\0'; p++) {
        if (*p == ',' && p[1] != '\0') {
            n++;
        }
    }
    return n;
}

static bool query_adc_layout(uint8_t *ch, uint32_t *mask)
{
    /* Korvo-2 ES7210 is "FL,FR,RE,NA" (4ch). get_data_layout() before I2S
       runs times out (0x106); falling back to 2ch then makes AFE/AUD_SRC -6. */
    dev_audio_codec_config_t *adc_cfg = NULL;
    if (esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
                                            (void **)&adc_cfg) == ESP_OK &&
        adc_cfg != NULL) {
        uint8_t n = channel_count_from_label(adc_cfg->codec_adc_cfg.label);
        if (n > 0) {
            if (ch != NULL) {
                *ch = n;
            }
            if (mask != NULL) {
                *mask = (1U << n) - 1U;
            }
            return true;
        }
    }

    esp_codec_dev_handle_t adc = codec_dev(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    esp_codec_dev_channel_map_t map = {0};
    if (adc == NULL || esp_codec_dev_get_data_layout(adc, &map) != ESP_CODEC_DEV_OK ||
        map.value == 0) {
        return false;
    }
    uint8_t n = 0;
    uint32_t m = 0;
    for (; n < 8; n++) {
        uint8_t slot = (map.value >> (4 * n)) & 0xFu;
        if (slot == 0) {
            break;
        }
        m |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(slot - 1);
    }
    if (n == 0) {
        return false;
    }
    if (ch != NULL) {
        *ch = n;
    }
    if (mask != NULL) {
        *mask = m;
    }
    return true;
}

static void apply_adc_gain(void)
{
    esp_codec_dev_handle_t adc = codec_dev(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    if (adc == NULL) {
        ESP_LOGW(TAG, "ADC gain skipped: audio ADC unavailable");
        return;
    }
    if (esp_codec_dev_set_in_gain(adc, VOLC_APP_MEDIA_ADC_MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set mic gain failed: %.1f dB", VOLC_APP_MEDIA_ADC_MIC_GAIN_DB);
    } else {
        ESP_LOGI(TAG, "Set mic gain %.1f dB", VOLC_APP_MEDIA_ADC_MIC_GAIN_DB);
    }
    uint8_t adc_ch = VOLC_APP_MEDIA_ADC_CH_FALLBACK;
    (void)query_adc_layout(&adc_ch, NULL);
    if (adc_ch <= 2) {
        return;
    }
    if (esp_codec_dev_set_in_channel_gain(adc, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(adc_ch - 1),
                                          VOLC_APP_MEDIA_ADC_REF_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set AEC ref gain failed");
    }
}

static esp_err_t create_audio_capture(const esp_llm_volc_cfg_t *volc_cfg, volc_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    ESP_RETURN_ON_ERROR(resolve_stream_info(volc_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &track_info),
                        TAG, "Resolve AUDIO_UP failed");

    esp_codec_dev_handle_t adc = codec_dev(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    if (adc != NULL) {
        (void)esp_codec_dev_set_disable_when_closed(adc, false);
    }
    apply_adc_gain();

    esp_audio_capture_service_cfg_t service_cfg = ESP_AUDIO_CAPTURE_SERVICE_CFG_DEFAULT();
    service_cfg.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    ESP_RETURN_ON_ERROR(esp_audio_capture_service_create(&service_cfg, &media->audio_capture),
                        TAG, "Create audio capture failed");

    uint32_t ai_mask = volc_cfg->service.audio_ai_cfg.feature_mask;
    if (ai_mask != 0) {
        ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_feature(media->audio_capture, ai_mask,
                           &volc_cfg->service.audio_ai_cfg.feature_cfg),
                           TAG, "Set capture AI features 0x%" PRIx32 " failed", ai_mask);
        ESP_LOGI(TAG, "Set capture AI features 0x%" PRIx32 " before setup", ai_mask);
    }

    /* G.711 uplink is 8 kHz; pin the ADC/AFE source to 16 kHz PCM. */
    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VOLC_APP_MEDIA_DEVICE_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = track_info.info.audio,
        },
    };
    ESP_RETURN_ON_ERROR(esp_audio_capture_service_apply_setup(media->audio_capture, &setup),
                        TAG, "Configure audio capture failed");
    return ESP_OK;
}

static esp_err_t create_audio_player(const esp_llm_volc_cfg_t *volc_cfg,
                                     const volc_app_media_cfg_t *cfg,
                                     volc_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    ESP_RETURN_ON_ERROR(resolve_stream_info(volc_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN, &track_info),
                        TAG, "Resolve AUDIO_DN failed");
    (void)track_info;

    esp_audio_player_service_cfg_t service_cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    service_cfg.max_stream_num = VOLC_APP_MEDIA_PLAYER_STREAM_NUM;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_create(&service_cfg, &media->audio_player),
                        TAG, "Create audio player failed");

    esp_audio_player_service_setup_t setup = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    setup.fixed_out_sample_info.sample_rate = VOLC_APP_MEDIA_DEVICE_SAMPLE_RATE;
    setup.fixed_out_sample_info.bits_per_sample = VOLC_APP_MEDIA_CODEC_BITS;
    setup.fixed_out_sample_info.channel = VOLC_APP_MEDIA_DAC_CHANNELS;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_apply_setup(media->audio_player, &setup),
                        TAG, "Configure audio player failed");

    ESP_RETURN_ON_ERROR(esp_player_service_set_output_volume(media->audio_player, cfg->output_volume),
                        TAG, "Set output volume %u failed", (unsigned)cfg->output_volume);
    return ESP_OK;
}

static void init_board_sdcard(void)
{
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD)) {
        return;
    }
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init SD card skipped: %s", esp_err_to_name(ret));
    }
}

static void deinit_board_sdcard(void)
{
    if (esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD)) {
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    }
}

static esp_err_t init_board_audio(void)
{
    init_board_sdcard();
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC),
                        TAG, "Init DAC failed");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC),
                        TAG, "Init ADC failed");
    ESP_RETURN_ON_FALSE(esp_audio_enc_register_default() == ESP_AUDIO_ERR_OK, ESP_FAIL, TAG,
                        "Register audio encoders failed");
    ESP_RETURN_ON_FALSE(esp_audio_dec_register_default() == ESP_AUDIO_ERR_OK, ESP_FAIL, TAG,
                        "Register audio decoders failed");
    ESP_RETURN_ON_ERROR(esp_extractor_register_default(), TAG, "Register extractors failed");
    return ESP_OK;
}

static void deinit_board_audio(void)
{
    esp_extractor_unregister_default();
    esp_audio_dec_unregister_default();
    esp_audio_enc_unregister_default();
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    deinit_board_sdcard();
}

static esp_err_t unlink_services(volc_app_media_t *media)
{
    if (media->llm == NULL) {
        return ESP_OK;
    }
    esp_err_t result = ESP_OK;
    esp_err_t ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                             ESP_SERVICE_BASE(media->audio_player), VOLC_APP_MEDIA_STREAM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink LLM from audio player failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
    ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->audio_capture), VOLC_APP_MEDIA_STREAM,
                                   ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink audio capture from LLM failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
    media->llm = NULL;
    return result;
}

static esp_err_t link_services(volc_app_media_t *media)
{
    esp_err_t ret = esp_media_service_link(ESP_SERVICE_BASE(media->audio_capture), VOLC_APP_MEDIA_STREAM,
                                           ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                     ESP_SERVICE_BASE(media->audio_player), VOLC_APP_MEDIA_STREAM);
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Link 'media_graph': capture to LLM to player");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Link media graph failed: %s", esp_err_to_name(ret));
    (void)unlink_services(media);
    return ret;
}

esp_err_t volc_app_media_create(const esp_llm_volc_cfg_t *volc_cfg,
                                 const volc_app_media_cfg_t *cfg,
                                 volc_app_media_t **out_media)
{
    if (volc_cfg == NULL || cfg == NULL || out_media == NULL || *out_media != NULL) {
        ESP_LOGE(TAG, "Create failed: invalid argument or output is not NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->output_volume > 100) {
        ESP_LOGE(TAG, "Create failed: output volume is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    volc_app_media_t *media = calloc(1, sizeof(*media));
    if (media == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for media context");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_service_scheduler_set_cb(capture_scheduler_cb, NULL);
    if (ret == ESP_OK) {
        ret = init_board_audio();
    }
    if (ret == ESP_OK) {
        ret = create_audio_capture(volc_cfg, media);
    }
    if (ret == ESP_OK) {
        ret = create_audio_player(volc_cfg, cfg, media);
    }
    if (ret != ESP_OK) {
        (void)volc_app_media_destroy(&media);
        return ret;
    }

    *out_media = media;
    ESP_LOGI(TAG, "Create 'media_graph'");
    return ESP_OK;
}

esp_err_t volc_app_media_get_audio_capture(volc_app_media_t *media,
                                            esp_capture_service_t **out_capture)
{
    if (media == NULL || out_capture == NULL) {
        ESP_LOGE(TAG, "Get audio capture failed: media or output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (media->audio_capture == NULL) {
        ESP_LOGE(TAG, "Get audio capture failed: service is unavailable");
        return ESP_ERR_NOT_FOUND;
    }
    *out_capture = media->audio_capture;
    return ESP_OK;
}

esp_err_t volc_app_media_stop(volc_app_media_t *media)
{
    if (media == NULL) {
        ESP_LOGE(TAG, "Stop failed: media is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    try_stop(media->audio_capture, "audio capture", &result);
    try_stop(media->llm, "LLM service", &result);
    try_stop(media->audio_player, "audio player", &result);
    record_first_error(&result, unlink_services(media));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Stop 'media_graph'");
    }
    return result;
}

esp_err_t volc_app_media_start(volc_app_media_t *media, esp_llm_service_t *llm)
{
    if (media == NULL || llm == NULL) {
        ESP_LOGE(TAG, "Start failed: media or LLM service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (media->llm != NULL) {
        ESP_LOGE(TAG, "Start failed: media graph is already active");
        return ESP_ERR_INVALID_STATE;
    }

    media->llm = llm;
    esp_err_t ret = link_services(media);
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->audio_player));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->llm));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->audio_capture));
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Start 'media_graph'");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Start media graph failed: %s", esp_err_to_name(ret));
    (void)volc_app_media_stop(media);
    return ret;
}

esp_err_t volc_app_media_destroy(volc_app_media_t **media)
{
    if (media == NULL || *media == NULL) {
        ESP_LOGE(TAG, "Destroy failed: media handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    volc_app_media_t *ctx = *media;
    esp_err_t result = volc_app_media_stop(ctx);
    if (ctx->audio_player != NULL) {
        record_first_error(&result, esp_player_service_destroy(ctx->audio_player));
        ctx->audio_player = NULL;
    }
    if (ctx->audio_capture != NULL) {
        record_first_error(&result, esp_capture_service_destroy(ctx->audio_capture));
        ctx->audio_capture = NULL;
    }
    deinit_board_audio();

    free(ctx);
    *media = NULL;
    ESP_LOGI(TAG, "Destroy 'media_graph'");
    return result;
}
