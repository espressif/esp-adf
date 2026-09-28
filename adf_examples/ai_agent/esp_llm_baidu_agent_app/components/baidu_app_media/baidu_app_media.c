/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <inttypes.h>
#include <stdlib.h>

#include "sdkconfig.h"

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

#if CONFIG_BAIDU_APP_SCENE_IMAGE_UNDERSTANDING || CONFIG_BAIDU_APP_SCENE_MULTI_MODAL
#define BAIDU_APP_MEDIA_CAMERA  1
#else
#define BAIDU_APP_MEDIA_CAMERA  0
#endif

#if CONFIG_BAIDU_APP_SCENE_IMAGE_GENERATION || CONFIG_BAIDU_APP_SCENE_MULTI_MODAL
#define BAIDU_APP_MEDIA_VIDEO_DN  1
#else
#define BAIDU_APP_MEDIA_VIDEO_DN  0
#endif

#define BAIDU_APP_MEDIA_LCD    BAIDU_APP_MEDIA_VIDEO_DN
#define BAIDU_APP_MEDIA_VIDEO  (BAIDU_APP_MEDIA_CAMERA || BAIDU_APP_MEDIA_LCD)

#if BAIDU_APP_MEDIA_VIDEO
#include "esp_video_dec_default.h"
#include "esp_video_enc_default.h"
#if BAIDU_APP_MEDIA_CAMERA
#include "esp_video_capture_service_setup.h"
#endif
#if BAIDU_APP_MEDIA_LCD
#include "esp_video_player_service_setup.h"
#endif
#endif

#include "baidu_app_media.h"

#define BAIDU_APP_MEDIA_STREAM               ESP_MEDIA_DEFAULT_STREAM
#define BAIDU_APP_MEDIA_AENC_TASK_STACK      (40 * 1024)
#define BAIDU_APP_MEDIA_AFE_FEED_STACK       (8 * 1024)
#define BAIDU_APP_MEDIA_AFE_FETCH_STACK      (12 * 1024)
#define BAIDU_APP_MEDIA_AFE_FEED_CORE        (1)
#define BAIDU_APP_MEDIA_AFE_FEED_PRIO        (15)
#define BAIDU_APP_MEDIA_AFE_FETCH_CORE       (0)
#define BAIDU_APP_MEDIA_AFE_FETCH_PRIO       (16)
#define BAIDU_APP_MEDIA_AENC_CORE            (1)
#define BAIDU_APP_MEDIA_AENC_PRIO            (5)
#define BAIDU_APP_MEDIA_ADC_MIC_GAIN_DB      (36.0f)
#define BAIDU_APP_MEDIA_ADC_REF_GAIN_DB      (20.0f)
#define BAIDU_APP_MEDIA_DAC_CHANNELS         (1)
#define BAIDU_APP_MEDIA_ADC_CH_FALLBACK      (2)
#define BAIDU_APP_MEDIA_CODEC_BITS           (16)

#if BAIDU_APP_MEDIA_CAMERA
#ifndef ESP_BOARD_DEVICE_NAME_CAMERA
#error "Baidu camera scenes require a board with CAMERA"
#endif
#endif

static const char *TAG = "BAIDU_APP_MEDIA";

/**
 * @brief  Private application media graph
 */
struct baidu_app_media {
    esp_capture_service_t  *audio_capture;  /*!< Application-owned audio capture */
    esp_player_service_t   *player;         /*!< Application-owned player (audio or video) */
    esp_llm_service_t      *llm;            /*!< Borrowed LLM while the graph is linked */
#if BAIDU_APP_MEDIA_CAMERA
    esp_capture_service_t  *video_capture;  /*!< Application-owned video capture */
    bool                    have_camera;    /*!< Camera present and initialized */
#endif
#if BAIDU_APP_MEDIA_LCD
    bool                    have_lcd;       /*!< LCD present and initialized */
#endif
#if BAIDU_APP_MEDIA_VIDEO
    bool                    video_codecs;   /*!< Default video enc/dec registered */
#endif
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
    /* video-rec create overwrites the one-shot capture scheduler name. Match
       audio threads by name so OPUS aenc_0 still gets 40K after Vision opens. */
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FEED)) {
        cfg->stack_size = BAIDU_APP_MEDIA_AFE_FEED_STACK;
        cfg->core_id = BAIDU_APP_MEDIA_AFE_FEED_CORE;
        cfg->priority = BAIDU_APP_MEDIA_AFE_FEED_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH)) {
        cfg->stack_size = BAIDU_APP_MEDIA_AFE_FETCH_STACK;
        cfg->core_id = BAIDU_APP_MEDIA_AFE_FETCH_CORE;
        cfg->priority = BAIDU_APP_MEDIA_AFE_FETCH_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_0) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_1)) {
        cfg->stack_size = BAIDU_APP_MEDIA_AENC_TASK_STACK;
        cfg->is_ext = true;
        cfg->core_id = BAIDU_APP_MEDIA_AENC_CORE;
        cfg->priority = BAIDU_APP_MEDIA_AENC_PRIO;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t resolve_stream_info(const esp_llm_baidu_cfg_t *baidu_cfg,
                                     esp_llm_media_stream_t stream,
                                     esp_media_track_info_t *out_info)
{
    if (baidu_cfg == NULL || out_info == NULL || stream >= ESP_LLM_MEDIA_STREAM_COUNT) {
        ESP_LOGE(TAG, "Resolve stream failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (!baidu_cfg->service.streams[stream].enabled) {
        ESP_LOGE(TAG, "Resolve stream failed: stream %d is disabled", (int)stream);
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = baidu_cfg->service.streams[stream].info;
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
    /* Board label (Korvo-2: "FL,FR,RE,NA" = 4ch) is valid before ADC I2S runs.
       get_data_layout() on a closed ADC times out (0x106); ignoring that used
       to open 2ch while capture later opens 4ch and AUD_SRC dies. */
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
    if (esp_codec_dev_set_in_gain(adc, BAIDU_APP_MEDIA_ADC_MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set mic gain failed: %.1f dB", BAIDU_APP_MEDIA_ADC_MIC_GAIN_DB);
    } else {
        ESP_LOGI(TAG, "Set mic gain %.1f dB", BAIDU_APP_MEDIA_ADC_MIC_GAIN_DB);
    }
    uint8_t adc_ch = BAIDU_APP_MEDIA_ADC_CH_FALLBACK;
    (void)query_adc_layout(&adc_ch, NULL);
    if (adc_ch <= 2) {
        return;
    }
    if (esp_codec_dev_set_in_channel_gain(adc, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(adc_ch - 1),
                                          BAIDU_APP_MEDIA_ADC_REF_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set AEC ref gain failed");
    }
}

static esp_err_t create_audio_capture(const esp_llm_baidu_cfg_t *baidu_cfg, baidu_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    esp_err_t ret = resolve_stream_info(baidu_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &track_info);
    if (ret != ESP_OK) {
        return ret;  /* resolve_stream_info already logged */
    }

    esp_codec_dev_handle_t adc = codec_dev(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    if (adc != NULL) {
        (void)esp_codec_dev_set_disable_when_closed(adc, false);
    }
    apply_adc_gain();

    esp_audio_capture_service_cfg_t service_cfg = ESP_AUDIO_CAPTURE_SERVICE_CFG_DEFAULT();
    service_cfg.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    ret = esp_audio_capture_service_create(&service_cfg, &media->audio_capture);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create audio capture failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint32_t ai_mask = baidu_cfg->service.audio_ai_cfg.feature_mask;
    if (ai_mask != 0) {
        ret = esp_capture_service_ai_audio_src_set_feature(media->audio_capture, ai_mask,
                                                           &baidu_cfg->service.audio_ai_cfg.feature_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set capture AI features 0x%" PRIx32 " failed: %s",
                     ai_mask, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Set capture AI features 0x%" PRIx32 " before setup", ai_mask);
    }

    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = track_info.info.audio.sample_rate,
        .streams[0] = {
            .enabled = true,
            .audio_info = track_info.info.audio,
        },
    };
    ret = esp_audio_capture_service_apply_setup(media->audio_capture, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configure audio capture failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t set_mix(baidu_app_media_t *media, esp_media_stream_id_t stream,
                         const esp_player_mix_cfg_t *mix, const char *name)
{
    esp_err_t ret = esp_player_service_set_mix_cfg(media->player, stream, mix);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set %s mix failed: %s", name, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t apply_player_mix_and_volume(baidu_app_media_t *media, const baidu_app_media_cfg_t *cfg)
{
    /* Same priority + COEXIST: TTS and URL mix. Prompt is URGENT EXCLUSIVE so it
       takes the DAC and the others make way (TTS DROP, URL PAUSE). */
    const esp_player_mix_cfg_t tts_mix = {
        .active_gain = 1.0f,
        .duck_gain = 0.2f,
        .transition_ms = 50,
        .priority = ESP_PLAYER_PRIO_NOTIFY,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    ESP_RETURN_ON_ERROR(set_mix(media, BAIDU_APP_MEDIA_TTS_STREAM, &tts_mix, "TTS"), TAG, "TTS mix");

    const esp_player_mix_cfg_t music_mix = {
        .active_gain = 1.0f,
        .duck_gain = 0.2f,
        .transition_ms = 50,
        .priority = ESP_PLAYER_PRIO_NOTIFY,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,
    };
    ESP_RETURN_ON_ERROR(set_mix(media, BAIDU_APP_MEDIA_MUSIC_STREAM, &music_mix, "URL"), TAG, "URL mix");

    const esp_player_mix_cfg_t wakeup_mix = {
        .active_gain = 1.0f,
        .duck_gain = 0.0f,
        .transition_ms = 1,
        .priority = ESP_PLAYER_PRIO_URGENT,
        .preempt_mode = ESP_PLAYER_PREEMPT_EXCLUSIVE,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,
    };
    ESP_RETURN_ON_ERROR(set_mix(media, BAIDU_APP_MEDIA_WAKEUP_STREAM, &wakeup_mix, "wake-up"),
                        TAG, "Wake-up mix");
    ESP_RETURN_ON_ERROR(esp_player_service_set_output_volume(media->player, cfg->output_volume),
                        TAG, "Set output volume %u failed", (unsigned)cfg->output_volume);
    return ESP_OK;
}

static esp_err_t apply_dac_format(baidu_app_media_t *media, const esp_media_track_info_t *track_info)
{
    esp_audio_player_service_setup_t setup = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    setup.fixed_out_sample_info.sample_rate = track_info->info.audio.sample_rate;
    setup.fixed_out_sample_info.bits_per_sample = track_info->info.audio.bits_per_sample;
    setup.fixed_out_sample_info.channel = BAIDU_APP_MEDIA_DAC_CHANNELS;
    esp_err_t ret = esp_audio_player_service_apply_setup(media->player, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configure audio player failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Player DAC %lu Hz, %u ch",
             (unsigned long)setup.fixed_out_sample_info.sample_rate,
             (unsigned)setup.fixed_out_sample_info.channel);
    return ESP_OK;
}

#if BAIDU_APP_MEDIA_CAMERA
static esp_err_t create_video_capture(const esp_llm_baidu_cfg_t *baidu_cfg, baidu_app_media_t *media)
{
    esp_media_track_info_t info = {0};
    esp_err_t ret = resolve_stream_info(baidu_cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP, &info);
    if (ret != ESP_OK) {
        return ret;  /* resolve_stream_info already logged */
    }

    esp_video_capture_service_cfg_t service_cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    service_cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    service_cfg.max_stream_num = 1;
    ret = esp_video_capture_service_create(&service_cfg, &media->video_capture);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create video capture failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_capture_video_info_t default_fmt = {0};
    ret = esp_video_capture_service_get_default_format(media->video_capture, &default_fmt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Camera source is not usable, skip vision uplink");
        (void)esp_capture_service_destroy(media->video_capture);
        media->video_capture = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "Camera default format 0x%x %ux%u @ %u fps",
             (unsigned)default_fmt.format_id, (unsigned)default_fmt.width,
             (unsigned)default_fmt.height, (unsigned)default_fmt.fps);

    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fb_num = 3,
    };
    setup.streams[0].enabled = true;
    setup.streams[0].video_info = info.info.video;
    ret = esp_video_capture_service_apply_setup(media->video_capture, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configure video capture failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
#endif  /* BAIDU_APP_MEDIA_CAMERA */

#if BAIDU_APP_MEDIA_LCD
static esp_err_t create_video_player(const esp_llm_baidu_cfg_t *baidu_cfg,
                                     const baidu_app_media_cfg_t *cfg,
                                     baidu_app_media_t *media)
{
    esp_media_track_info_t audio = {0};
    esp_err_t ret = resolve_stream_info(baidu_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN, &audio);
    if (ret != ESP_OK) {
        return ret;  /* resolve_stream_info already logged */
    }

    uint16_t fps = 10;
#if BAIDU_APP_MEDIA_VIDEO_DN
    esp_media_track_info_t video = {0};
    ret = resolve_stream_info(baidu_cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN, &video);
    if (ret != ESP_OK) {
        return ret;  /* resolve_stream_info already logged */
    }
    if (video.info.video.fps != 0) {
        fps = video.info.video.fps;
    }
#endif

    esp_video_player_service_cfg_t service_cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    service_cfg.max_stream_num = BAIDU_APP_MEDIA_PLAYER_STREAM_NUM;
    service_cfg.render_fps = fps;
    ret = esp_video_player_service_create(&service_cfg, &media->player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create video player failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Cache 16 kHz DAC first. Video apply_setup uses that cache; otherwise it
       opens the DAC at the player default (48 kHz / 2 ch) and AFE capture
       (16 kHz) times out. */
    ret = apply_dac_format(media, &audio);
    if (ret != ESP_OK) {
        return ret;  /* apply_dac_format already logged */
    }

    esp_video_player_service_setup_t setup = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    setup.display_dev_name = ESP_BOARD_DEVICE_NAME_DISPLAY_LCD;
    ret = esp_video_player_service_apply_setup(media->player, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configure video player failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return apply_player_mix_and_volume(media, cfg);
}
#endif  /* BAIDU_APP_MEDIA_LCD */

static esp_err_t create_audio_player(const esp_llm_baidu_cfg_t *baidu_cfg,
                                     const baidu_app_media_cfg_t *cfg,
                                     baidu_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    esp_err_t ret = resolve_stream_info(baidu_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN, &track_info);
    if (ret != ESP_OK) {
        return ret;  /* resolve_stream_info already logged */
    }

    esp_audio_player_service_cfg_t service_cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    service_cfg.max_stream_num = BAIDU_APP_MEDIA_PLAYER_STREAM_NUM;
    ret = esp_audio_player_service_create(&service_cfg, &media->player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create audio player failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = apply_dac_format(media, &track_info);
    if (ret != ESP_OK) {
        return ret;  /* apply_dac_format already logged */
    }
    ESP_LOGI(TAG, "Player streams %u", (unsigned)service_cfg.max_stream_num);
    return apply_player_mix_and_volume(media, cfg);
}

#if BAIDU_APP_MEDIA_LCD
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
static void init_board_lcd_backlight(void)
{
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS)) {
        return;
    }
    if (esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS) != ESP_OK) {
        ESP_LOGW(TAG, "Init LCD backlight skipped");
        return;
    }
    periph_ledc_handle_t *ledc = NULL;
    dev_ledc_ctrl_config_t *cfg = NULL;
    periph_ledc_config_t *periph_cfg = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&ledc) != ESP_OK ||
        esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&cfg) != ESP_OK ||
        ledc == NULL || cfg == NULL ||
        esp_board_manager_get_periph_config(cfg->ledc_name, (void **)&periph_cfg) != ESP_OK ||
        periph_cfg == NULL) {
        ESP_LOGW(TAG, "LCD backlight handle missing");
        return;
    }
    uint32_t duty = (1U << (uint32_t)periph_cfg->duty_resolution) - 1U;
    if (ledc_set_duty(ledc->speed_mode, ledc->channel, duty) != ESP_OK ||
        ledc_update_duty(ledc->speed_mode, ledc->channel) != ESP_OK) {
        ESP_LOGW(TAG, "Set LCD backlight failed");
    }
}

static void deinit_board_lcd_backlight(void)
{
    if (esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS)) {
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
    }
}
#endif  /* CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT */
#endif  /* BAIDU_APP_MEDIA_LCD */

#if BAIDU_APP_MEDIA_VIDEO
static void init_video_devices(baidu_app_media_t *media)
{
#if BAIDU_APP_MEDIA_CAMERA
#if CONFIG_IDF_TARGET_ESP32P4
    (void)esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
#endif
    media->have_camera = esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (!media->have_camera) {
        ESP_LOGW(TAG, "Camera missing from board, skip vision uplink");
    } else {
        esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Init camera skipped: %s", esp_err_to_name(ret));
            media->have_camera = false;
        }
    }
#endif
#if BAIDU_APP_MEDIA_LCD
    media->have_lcd = esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (!media->have_lcd) {
        ESP_LOGW(TAG, "LCD missing from board, skip image playback");
    } else {
        esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Init LCD skipped: %s", esp_err_to_name(ret));
            media->have_lcd = false;
        } else {
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
            init_board_lcd_backlight();
#endif
        }
    }
#endif
    bool need_codecs = false;
#if BAIDU_APP_MEDIA_CAMERA
    need_codecs = need_codecs || media->have_camera;
#endif
#if BAIDU_APP_MEDIA_LCD
    need_codecs = need_codecs || media->have_lcd;
#endif
    if (need_codecs &&
        esp_video_enc_register_default() == ESP_VC_ERR_OK &&
        esp_video_dec_register_default() == ESP_VC_ERR_OK) {
        media->video_codecs = true;
    } else if (need_codecs) {
        ESP_LOGW(TAG, "Register video codecs failed, skip vision");
#if BAIDU_APP_MEDIA_CAMERA
        media->have_camera = false;
#endif
#if BAIDU_APP_MEDIA_LCD
        media->have_lcd = false;
#endif
    }
}
#endif  /* BAIDU_APP_MEDIA_VIDEO */

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

esp_err_t baidu_app_media_create(const esp_llm_baidu_cfg_t *baidu_cfg,
                                 const baidu_app_media_cfg_t *cfg,
                                 baidu_app_media_t **out_media)
{
    if (baidu_cfg == NULL || cfg == NULL || out_media == NULL || *out_media != NULL) {
        ESP_LOGE(TAG, "Create failed: invalid argument or output is not NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->output_volume > 100) {
        ESP_LOGE(TAG, "Create failed: output volume is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    baidu_app_media_t *media = calloc(1, sizeof(*media));
    if (media == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for media context");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_service_scheduler_set_cb(capture_scheduler_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Install capture scheduler failed: %s", esp_err_to_name(ret));
        free(media);
        return ret;
    }

    init_board_sdcard();
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    if (ret == ESP_OK) {
        ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    }
    if (ret == ESP_OK) {
        if (esp_audio_enc_register_default() != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "Register default audio encoders failed");
            ret = ESP_FAIL;
        }
    }
    if (ret == ESP_OK) {
        if (esp_audio_dec_register_default() != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "Register default audio decoders failed");
            ret = ESP_FAIL;
        }
    }
    if (ret == ESP_OK) {
        ret = esp_extractor_register_default();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Register default extractors failed: %s", esp_err_to_name(ret));
        }
    }
    if (ret == ESP_OK) {
        ret = create_audio_capture(baidu_cfg, media);
    }
#if BAIDU_APP_MEDIA_VIDEO
    if (ret == ESP_OK) {
        init_video_devices(media);
    }
#if BAIDU_APP_MEDIA_CAMERA
    if (ret == ESP_OK && media->have_camera) {
        ret = create_video_capture(baidu_cfg, media);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Video capture skipped: %s", esp_err_to_name(ret));
            if (media->video_capture != NULL) {
                (void)esp_capture_service_destroy(media->video_capture);
                media->video_capture = NULL;
            }
            ret = ESP_OK;
        }
    }
#endif
#endif
    if (ret == ESP_OK) {
#if BAIDU_APP_MEDIA_LCD
        if (media->have_lcd) {
            ret = create_video_player(baidu_cfg, cfg, media);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "LCD player skipped: %s", esp_err_to_name(ret));
                ret = create_audio_player(baidu_cfg, cfg, media);
            }
        } else {
            ret = create_audio_player(baidu_cfg, cfg, media);
        }
#else
        ret = create_audio_player(baidu_cfg, cfg, media);
#endif
    }
    if (ret != ESP_OK) {
        (void)baidu_app_media_destroy(&media);
        return ret;
    }

    *out_media = media;
    ESP_LOGI(TAG, "Create 'media_graph'");
    return ESP_OK;
}

esp_err_t baidu_app_media_get_audio_capture(baidu_app_media_t *media,
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

esp_err_t baidu_app_media_get_player(baidu_app_media_t *media,
                                     esp_player_service_t **out_player)
{
    if (media == NULL || out_player == NULL) {
        ESP_LOGE(TAG, "Get player failed: media or output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (media->player == NULL) {
        ESP_LOGE(TAG, "Get player failed: service is unavailable");
        return ESP_ERR_NOT_FOUND;
    }

    *out_player = media->player;
    return ESP_OK;
}

static esp_err_t unlink_services(baidu_app_media_t *media)
{
    if (media->llm == NULL) {
        return ESP_OK;
    }
    esp_err_t result = ESP_OK;
    esp_err_t ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->llm),
                                             ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                             ESP_SERVICE_BASE(media->player),
                                             BAIDU_APP_MEDIA_STREAM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink LLM from player failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
#if BAIDU_APP_MEDIA_CAMERA
    if (media->video_capture != NULL) {
        ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->video_capture),
                                       BAIDU_APP_MEDIA_STREAM,
                                       ESP_SERVICE_BASE(media->llm),
                                       ESP_LLM_MEDIA_STREAM_VIDEO_UP);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Unlink video capture from LLM failed: %s", esp_err_to_name(ret));
            record_first_error(&result, ret);
        }
    }
#endif
    ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->audio_capture),
                                   BAIDU_APP_MEDIA_STREAM,
                                   ESP_SERVICE_BASE(media->llm),
                                   ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink audio capture from LLM failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
    media->llm = NULL;
    return result;
}

static esp_err_t link_services(baidu_app_media_t *media)
{
    esp_err_t ret = esp_media_service_link(ESP_SERVICE_BASE(media->audio_capture),
                                           BAIDU_APP_MEDIA_STREAM,
                                           ESP_SERVICE_BASE(media->llm),
                                           ESP_LLM_MEDIA_STREAM_AUDIO_UP);
#if BAIDU_APP_MEDIA_CAMERA
    if (ret == ESP_OK && media->video_capture != NULL) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(media->video_capture),
                                     BAIDU_APP_MEDIA_STREAM,
                                     ESP_SERVICE_BASE(media->llm),
                                     ESP_LLM_MEDIA_STREAM_VIDEO_UP);
    }
#endif
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(media->llm),
                                     ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                     ESP_SERVICE_BASE(media->player),
                                     BAIDU_APP_MEDIA_STREAM);
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Link 'media_graph': capture to LLM to player");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Link media graph failed: %s", esp_err_to_name(ret));
    (void)unlink_services(media);
    return ret;
}

esp_err_t baidu_app_media_stop(baidu_app_media_t *media)
{
    if (media == NULL) {
        ESP_LOGE(TAG, "Stop failed: media is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
#if BAIDU_APP_MEDIA_CAMERA
    try_stop(media->video_capture, "video capture", &result);
#endif
    try_stop(media->audio_capture, "audio capture", &result);
    try_stop(media->llm, "LLM service", &result);
    try_stop(media->player, "player", &result);
    record_first_error(&result, unlink_services(media));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Stop 'media_graph'");
    }
    return result;
}

esp_err_t baidu_app_media_start(baidu_app_media_t *media, esp_llm_service_t *llm)
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
        ret = esp_service_start(ESP_SERVICE_BASE(media->player));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->llm));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->audio_capture));
    }
#if BAIDU_APP_MEDIA_CAMERA
    if (ret == ESP_OK && media->video_capture != NULL) {
        ret = esp_service_start(ESP_SERVICE_BASE(media->video_capture));
    }
#endif
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Start 'media_graph'");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Start media graph failed: %s", esp_err_to_name(ret));
    (void)baidu_app_media_stop(media);
    return ret;
}

esp_err_t baidu_app_media_destroy(baidu_app_media_t **media)
{
    if (media == NULL || *media == NULL) {
        ESP_LOGE(TAG, "Destroy failed: media handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    baidu_app_media_t *ctx = *media;
    esp_err_t result = baidu_app_media_stop(ctx);

    if (ctx->player != NULL) {
        record_first_error(&result, esp_player_service_destroy(ctx->player));
        ctx->player = NULL;
    }
#if BAIDU_APP_MEDIA_CAMERA
    if (ctx->video_capture != NULL) {
        record_first_error(&result, esp_capture_service_destroy(ctx->video_capture));
        ctx->video_capture = NULL;
    }
#endif
    if (ctx->audio_capture != NULL) {
        record_first_error(&result, esp_capture_service_destroy(ctx->audio_capture));
        ctx->audio_capture = NULL;
    }
#if BAIDU_APP_MEDIA_VIDEO
    if (ctx->video_codecs) {
        esp_video_dec_unregister_default();
        esp_video_enc_unregister_default();
    }
#endif
#if BAIDU_APP_MEDIA_LCD
    if (ctx->have_lcd) {
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
        deinit_board_lcd_backlight();
#endif
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    }
#endif
#if BAIDU_APP_MEDIA_CAMERA
    if (ctx->have_camera) {
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    }
#endif
    esp_extractor_unregister_default();
    esp_audio_dec_unregister_default();
    esp_audio_enc_unregister_default();
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    deinit_board_sdcard();

    free(ctx);
    *media = NULL;
    ESP_LOGI(TAG, "Destroy 'media_graph'");
    return result;
}
