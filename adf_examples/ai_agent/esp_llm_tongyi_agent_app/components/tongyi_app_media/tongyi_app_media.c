/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

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

#if CONFIG_TONGYI_APP_MODE_VISION
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_gmf_oal_thread.h"
#include "esp_media_provider.h"
#include "esp_video_capture_service_setup.h"
#include "esp_video_dec_default.h"
#include "esp_video_enc_default.h"
#include "esp_video_player_service_setup.h"
#endif

#include "tongyi_app_media.h"

#define TONGYI_APP_MEDIA_STREAM              ESP_MEDIA_DEFAULT_STREAM
#define TONGYI_APP_MEDIA_PLAYER_STREAM_NUM   1
#define TONGYI_APP_MEDIA_AENC_TASK_STACK     (40 * 1024)
#define TONGYI_APP_MEDIA_AFE_FEED_STACK      (8 * 1024)
#define TONGYI_APP_MEDIA_AFE_FETCH_STACK     (12 * 1024)
#define TONGYI_APP_MEDIA_AFE_FEED_CORE       (1)
#define TONGYI_APP_MEDIA_AFE_FEED_PRIO       (15)
#define TONGYI_APP_MEDIA_AFE_FETCH_CORE      (0)
#define TONGYI_APP_MEDIA_AFE_FETCH_PRIO      (16)
#define TONGYI_APP_MEDIA_AENC_CORE           (1)
#define TONGYI_APP_MEDIA_AENC_PRIO           (5)
#define TONGYI_APP_MEDIA_DEVICE_SAMPLE_RATE  (16000U)
#define TONGYI_APP_MEDIA_ADC_MIC_GAIN_DB     (32.0f)
#define TONGYI_APP_MEDIA_ADC_REF_GAIN_DB     (20.0f)
#define TONGYI_APP_MEDIA_DAC_CHANNELS        (1)
#define TONGYI_APP_MEDIA_ADC_CH_FALLBACK     (2)
#define TONGYI_APP_MEDIA_CODEC_BITS          (16)

#if CONFIG_TONGYI_APP_MODE_VISION
#define TONGYI_JPEG_STREAM               ESP_MEDIA_DEFAULT_STREAM
#define TONGYI_PREVIEW_STREAM            ((esp_media_stream_id_t)1)
#define TONGYI_CAM_W                     ((uint16_t)(CONFIG_TONGYI_VISION_CAMERA_WIDTH & ~1))
#define TONGYI_CAM_H                     ((uint16_t)(CONFIG_TONGYI_VISION_CAMERA_HEIGHT & ~1))
#define TONGYI_PREVIEW_FPS               ((uint8_t)CONFIG_TONGYI_VISION_CAMERA_FPS)
#define TONGYI_PREVIEW_STACK             (16 * 1024)
#define TONGYI_PREVIEW_PRIO              (7)
#define TONGYI_PREVIEW_STACK_IN_EXT      (true)
#define TONGYI_PREVIEW_CORE_ID           (0)
#define TONGYI_PREVIEW_JOIN_TRIES        (50)
#define TONGYI_PREVIEW_JOIN_DELAY_MS     (20)
#define TONGYI_V4L2_FB_NUM               (4)
#endif

static const char *TAG = "TONGYI_APP_MEDIA";

/**
 * @brief  Private application media graph
 */
struct tongyi_app_media {
    esp_capture_service_t  *audio_capture;  /*!< Application-owned audio capture */
    esp_player_service_t   *audio_player;   /*!< Application-owned audio player */
    esp_llm_service_t      *llm;            /*!< Borrowed LLM while the graph is linked */
#if CONFIG_TONGYI_APP_MODE_VISION
    esp_capture_service_t            *video_capture;   /*!< JPEG capture */
    esp_player_service_t             *video_player;    /*!< Owns the LCD render */
    esp_video_render_stream_handle_t  preview_stream;  /*!< RGB565 render stream */
    esp_gmf_oal_thread_t              preview_task;    /*!< JPEG preview plus cache thread */
    uint8_t                          *jpeg_cache;      /*!< Last complete JPEG for snap / UpdateInfo */
    size_t                            jpeg_cache_size; /*!< Cached JPEG size */
    SemaphoreHandle_t                 jpeg_lock;       /*!< Guards jpeg_cache */
    volatile bool                     preview_run;     /*!< Preview task stay-alive flag */
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
        cfg->stack_size = TONGYI_APP_MEDIA_AFE_FEED_STACK;
        cfg->core_id = TONGYI_APP_MEDIA_AFE_FEED_CORE;
        cfg->priority = TONGYI_APP_MEDIA_AFE_FEED_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH)) {
        cfg->stack_size = TONGYI_APP_MEDIA_AFE_FETCH_STACK;
        cfg->core_id = TONGYI_APP_MEDIA_AFE_FETCH_CORE;
        cfg->priority = TONGYI_APP_MEDIA_AFE_FETCH_PRIO;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_0) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_1)) {
        cfg->stack_size = TONGYI_APP_MEDIA_AENC_TASK_STACK;
        cfg->is_ext = true;
        cfg->core_id = TONGYI_APP_MEDIA_AENC_CORE;
        cfg->priority = TONGYI_APP_MEDIA_AENC_PRIO;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t resolve_stream_info(const esp_llm_tongyi_cfg_t *tongyi_cfg,
                                     esp_llm_media_stream_t stream,
                                     esp_media_track_info_t *out_info)
{
    if (tongyi_cfg == NULL || out_info == NULL || stream >= ESP_LLM_MEDIA_STREAM_COUNT) {
        ESP_LOGE(TAG, "Resolve stream failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (!tongyi_cfg->service.streams[stream].enabled) {
        ESP_LOGE(TAG, "Resolve stream failed: stream %d is disabled", (int)stream);
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = tongyi_cfg->service.streams[stream].info;
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
    if (esp_codec_dev_set_in_gain(adc, TONGYI_APP_MEDIA_ADC_MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set mic gain failed: %.1f dB", TONGYI_APP_MEDIA_ADC_MIC_GAIN_DB);
    } else {
        ESP_LOGI(TAG, "Set mic gain %.1f dB", TONGYI_APP_MEDIA_ADC_MIC_GAIN_DB);
    }
    uint8_t adc_ch = TONGYI_APP_MEDIA_ADC_CH_FALLBACK;
    (void)query_adc_layout(&adc_ch, NULL);
    if (adc_ch <= 2) {
        return;
    }
    if (esp_codec_dev_set_in_channel_gain(adc, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(adc_ch - 1),
                                          TONGYI_APP_MEDIA_ADC_REF_GAIN_DB) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set AEC ref gain failed");
    }
}

static esp_err_t create_audio_capture(const esp_llm_tongyi_cfg_t *tongyi_cfg, tongyi_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    ESP_RETURN_ON_ERROR(resolve_stream_info(tongyi_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &track_info),
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

    uint32_t ai_mask = tongyi_cfg->service.audio_ai_cfg.feature_mask;
    if (ai_mask != 0) {
        ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_feature(media->audio_capture, ai_mask,
                           &tongyi_cfg->service.audio_ai_cfg.feature_cfg),
                           TAG, "Set capture AI features 0x%" PRIx32 " failed", ai_mask);
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
    ESP_RETURN_ON_ERROR(esp_audio_capture_service_apply_setup(media->audio_capture, &setup),
                        TAG, "Configure audio capture failed");
    return ESP_OK;
}

static esp_err_t create_audio_player(const esp_llm_tongyi_cfg_t *tongyi_cfg,
                                     const tongyi_app_media_cfg_t *cfg,
                                     tongyi_app_media_t *media)
{
    esp_media_track_info_t track_info = {0};
    ESP_RETURN_ON_ERROR(resolve_stream_info(tongyi_cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN, &track_info),
                        TAG, "Resolve AUDIO_DN failed");

    esp_audio_player_service_cfg_t service_cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    service_cfg.max_stream_num = TONGYI_APP_MEDIA_PLAYER_STREAM_NUM;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_create(&service_cfg, &media->audio_player),
                        TAG, "Create audio player failed");

    esp_audio_player_service_setup_t setup = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    setup.fixed_out_sample_info.sample_rate = track_info.info.audio.sample_rate;
    setup.fixed_out_sample_info.bits_per_sample = track_info.info.audio.bits_per_sample;
    setup.fixed_out_sample_info.channel = TONGYI_APP_MEDIA_DAC_CHANNELS;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_apply_setup(media->audio_player, &setup),
                        TAG, "Configure audio player failed");

    ESP_RETURN_ON_ERROR(esp_player_service_set_output_volume(media->audio_player, cfg->output_volume),
                        TAG, "Set output volume %u failed", (unsigned)cfg->output_volume);
    return ESP_OK;
}

#if CONFIG_TONGYI_APP_MODE_VISION
static bool jpeg_is_complete(const uint8_t *data, size_t size)
{
    return data != NULL && size >= 4 && data[0] == 0xFF && data[1] == 0xD8 &&
           data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

static void jpeg_cache_store(tongyi_app_media_t *media, const uint8_t *data, size_t size)
{
    if (media == NULL || media->jpeg_lock == NULL || !jpeg_is_complete(data, size)) {
        return;
    }
    uint8_t *copy = malloc(size);
    if (copy == NULL) {
        return;
    }
    memcpy(copy, data, size);
    if (xSemaphoreTake(media->jpeg_lock, pdMS_TO_TICKS(20)) != pdTRUE) {
        free(copy);
        return;
    }
    free(media->jpeg_cache);
    media->jpeg_cache = copy;
    media->jpeg_cache_size = size;
    xSemaphoreGive(media->jpeg_lock);
}

static void vision_abort(esp_capture_service_t *cap, esp_media_stream_id_t stream)
{
    esp_media_provider_t provider = {0};
    if (cap != NULL &&
        esp_media_service_get_provider(ESP_SERVICE_BASE(cap), stream, &provider) == ESP_OK) {
        (void)esp_media_provider_abort(&provider);
    }
}

static void vision_preview_task(void *arg)
{
    tongyi_app_media_t *media = (tongyi_app_media_t *)arg;
    esp_media_provider_t jpeg = {0};
    esp_media_provider_t preview = {0};
    if (esp_media_service_get_provider(ESP_SERVICE_BASE(media->video_capture), TONGYI_JPEG_STREAM,
                                       &jpeg) != ESP_OK ||
        esp_media_service_get_provider(ESP_SERVICE_BASE(media->video_capture), TONGYI_PREVIEW_STREAM,
                                       &preview) != ESP_OK) {
        ESP_LOGE(TAG, "Get vision providers failed");
        media->preview_task = NULL;
        esp_gmf_oal_thread_delete(NULL);
        return;
    }

    uint32_t shown = 0;
    TickType_t fps_tick = xTaskGetTickCount();
    const size_t rgb_copy_bytes = (size_t)TONGYI_CAM_W * (size_t)TONGYI_CAM_H * 2U;
    uint8_t *rgb_copy = malloc(rgb_copy_bytes);
    if (rgb_copy == NULL) {
        ESP_LOGE(TAG, "No memory for RGB565 preview copy");
        media->preview_task = NULL;
        esp_gmf_oal_thread_delete(NULL);
        return;
    }

    while (media->preview_run) {
        /* Drain JPEG so the shared source does not stall the RGB path. */
        for (;;) {
            esp_media_frame_t jpeg_frame = { .type = ESP_MEDIA_TRACK_TYPE_VIDEO };
            if (esp_media_provider_acquire_frame(&jpeg, &jpeg_frame, 0) != ESP_OK) {
                break;
            }
            jpeg_cache_store(media, jpeg_frame.data, jpeg_frame.size);
            (void)esp_media_provider_release_frame(&jpeg, &jpeg_frame);
        }

        esp_media_frame_t rgb = { .type = ESP_MEDIA_TRACK_TYPE_VIDEO };
        if (esp_media_provider_acquire_frame(&preview, &rgb, 200) != ESP_OK) {
            continue;
        }
        size_t rgb_size = rgb.size;
        bool have_copy = false;
        if (rgb.data != NULL && rgb_size > 0 && rgb_size <= rgb_copy_bytes) {
            memcpy(rgb_copy, rgb.data, rgb_size);
            have_copy = true;
        }
        (void)esp_media_provider_release_frame(&preview, &rgb);
        if (media->preview_stream != NULL && have_copy) {
            esp_video_render_frame_t render = {
                .format = ESP_VIDEO_RENDER_FORMAT_RGB565,
                .width = TONGYI_CAM_W,
                .height = TONGYI_CAM_H,
                .data = rgb_copy,
                .size = rgb_size,
            };
            if (esp_video_render_stream_write(media->preview_stream, &render) != ESP_VIDEO_RENDER_ERR_OK) {
                ESP_LOGW(TAG, "LCD write failed");
            } else {
                shown++;
            }
        }
        TickType_t now = xTaskGetTickCount();
        if ((now - fps_tick) >= pdMS_TO_TICKS(2000)) {
            ESP_LOGI(TAG, "LCD preview %lu fps (target %u)",
                     (unsigned long)(shown * 1000 / 2000), (unsigned)TONGYI_PREVIEW_FPS);
            shown = 0;
            fps_tick = now;
        }
    }
    free(rgb_copy);
    media->preview_task = NULL;
    esp_gmf_oal_thread_delete(NULL);
}

static void vision_stop(tongyi_app_media_t *media)
{
    media->preview_run = false;
    vision_abort(media->video_capture, TONGYI_PREVIEW_STREAM);
    vision_abort(media->video_capture, TONGYI_JPEG_STREAM);
    for (int i = 0; i < TONGYI_PREVIEW_JOIN_TRIES && media->preview_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(TONGYI_PREVIEW_JOIN_DELAY_MS));
    }
    try_stop(media->video_capture, "video capture", NULL);
    try_stop(media->video_player, "LCD player", NULL);
}

static esp_err_t vision_start(tongyi_app_media_t *media)
{
    if (media->video_capture == NULL || media->video_player == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(media->video_player)), TAG, "Start LCD failed");
    ESP_RETURN_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(media->video_capture)), TAG, "Start camera failed");
    media->preview_run = true;
    if (esp_gmf_oal_thread_create(&media->preview_task, "tongyi_prev", vision_preview_task, media,
                                  TONGYI_PREVIEW_STACK, TONGYI_PREVIEW_PRIO,
                                  TONGYI_PREVIEW_STACK_IN_EXT, TONGYI_PREVIEW_CORE_ID) != ESP_GMF_ERR_OK) {
        media->preview_run = false;
        media->preview_task = NULL;
        ESP_LOGE(TAG, "Create preview thread failed: GMF thread create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

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
#endif

static void vision_close(tongyi_app_media_t *media)
{
    vision_stop(media);
    if (media->preview_stream != NULL) {
        (void)esp_video_render_stream_close(media->preview_stream);
        media->preview_stream = NULL;
    }
    if (media->jpeg_lock != NULL) {
        (void)xSemaphoreTake(media->jpeg_lock, pdMS_TO_TICKS(50));
        free(media->jpeg_cache);
        media->jpeg_cache = NULL;
        media->jpeg_cache_size = 0;
        xSemaphoreGive(media->jpeg_lock);
        vSemaphoreDelete(media->jpeg_lock);
        media->jpeg_lock = NULL;
    }
    if (media->video_player != NULL) {
        (void)esp_player_service_destroy(media->video_player);
        media->video_player = NULL;
    }
    if (media->video_capture != NULL) {
        (void)esp_capture_service_destroy(media->video_capture);
        media->video_capture = NULL;
    }
    esp_video_dec_unregister_default();
    esp_video_enc_unregister_default();
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
    deinit_board_lcd_backlight();
#endif
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
}

static esp_err_t vision_open_preview_stream(tongyi_app_media_t *media, esp_video_render_handle_t render)
{
    esp_video_render_disp_info_t disp = {0};
    ESP_RETURN_ON_FALSE(esp_video_render_get_display_info(render, &disp) == ESP_VIDEO_RENDER_ERR_OK &&
                        disp.width > 0 && disp.height > 0,
                        ESP_FAIL, TAG, "Get LCD display info failed");

    /* fps=0: do not pace stream_write at CAMERA_FPS. S3 SW JPEG + SPI LCD
       lands a little under 15; the render rate-control would then reset
       every second with "Write too slow". One stream writes the panel
       directly — skip render_async so a blender thread is not also
       redrawing at render_fps and logging "Render too slow". */
    esp_video_render_stream_info_t stream_info = {
        .info = {
            .format = ESP_VIDEO_RENDER_FORMAT_RGB565,
            .width = TONGYI_CAM_W,
            .height = TONGYI_CAM_H,
            .fps = 0,
        },
    };
    ESP_RETURN_ON_FALSE(esp_video_render_stream_open(render, &stream_info,
                                                     &media->preview_stream) == ESP_VIDEO_RENDER_ERR_OK,
                        ESP_FAIL, TAG, "Open preview stream failed");
    esp_video_render_rect_t rect = {
        .x = 0,
        .y = 0,
        .width = disp.width,
        .height = disp.height,
    };
    if (esp_video_render_stream_set_disp_rect(media->preview_stream, &rect) != ESP_VIDEO_RENDER_ERR_OK) {
        ESP_LOGE(TAG, "Set LCD dest rect %ux%u failed", (unsigned)disp.width, (unsigned)disp.height);
        (void)esp_video_render_stream_close(media->preview_stream);
        media->preview_stream = NULL;
        return ESP_FAIL;
    }
    (void)esp_video_render_stream_set_zorder(media->preview_stream, 0);
    ESP_LOGI(TAG, "Vision ready: camera RGB565 %ux%u @ %u fps -> LCD %ux%u (direct write)",
             TONGYI_CAM_W, TONGYI_CAM_H, (unsigned)TONGYI_PREVIEW_FPS,
             (unsigned)disp.width, (unsigned)disp.height);
    return ESP_OK;
}

static esp_err_t vision_open(tongyi_app_media_t *media)
{
    bool have_lcd = esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    bool have_camera = esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (!have_lcd || !have_camera) {
        ESP_LOGW(TAG, "Vision skipped: LCD %s, camera %s",
                 have_lcd ? "present" : "missing", have_camera ? "present" : "missing");
        return ESP_ERR_NOT_FOUND;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    (void)esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
#endif
    ESP_RETURN_ON_FALSE(TONGYI_CAM_W && TONGYI_CAM_H, ESP_ERR_INVALID_ARG, TAG, "Invalid camera size");
    media->jpeg_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(media->jpeg_lock != NULL, ESP_ERR_NO_MEM, TAG, "No memory for JPEG cache lock");
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init LCD skipped: %s", esp_err_to_name(ret));
        return ret;
    }
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
    init_board_lcd_backlight();
#endif
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init camera skipped: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_RETURN_ON_FALSE(esp_video_enc_register_default() == ESP_VC_ERR_OK, ESP_FAIL, TAG,
                        "Register video encoders failed");
    ESP_RETURN_ON_FALSE(esp_video_dec_register_default() == ESP_VC_ERR_OK, ESP_FAIL, TAG,
                        "Register video decoders failed");

    esp_video_capture_service_cfg_t cap_cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    cap_cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    cap_cfg.max_stream_num = 2;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&cap_cfg, &media->video_capture),
                        TAG, "Create video capture failed");
    esp_capture_video_info_t default_fmt = {0};
    ESP_RETURN_ON_ERROR(esp_video_capture_service_get_default_format(media->video_capture, &default_fmt),
                        TAG, "Camera source is not usable by current capture");
    ESP_LOGI(TAG, "Camera default format 0x%x %ux%u @ %u fps",
             (unsigned)default_fmt.format_id, (unsigned)default_fmt.width,
             (unsigned)default_fmt.height, (unsigned)default_fmt.fps);
    esp_video_capture_service_setup_t cap_setup = { .stream_num = 2, .fb_num = TONGYI_V4L2_FB_NUM };
    cap_setup.streams[0].enabled = true;
    /* Product sinks stay JPEG + RGB565; GMF converts from the native source. */
    cap_setup.streams[0].video_info = (esp_media_video_info_t){
        .codec = ESP_CAPTURE_FMT_ID_MJPEG, .width = TONGYI_CAM_W, .height = TONGYI_CAM_H,
        .fps = TONGYI_PREVIEW_FPS,
    };
    cap_setup.streams[1].enabled = true;
    cap_setup.streams[1].video_info = (esp_media_video_info_t){
        .codec = ESP_CAPTURE_FMT_ID_RGB565, .width = TONGYI_CAM_W, .height = TONGYI_CAM_H,
        .fps = TONGYI_PREVIEW_FPS,
    };
    ESP_RETURN_ON_ERROR(esp_video_capture_service_apply_setup(media->video_capture, &cap_setup),
                        TAG, "Configure video capture failed");

    esp_video_player_service_cfg_t play_cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    play_cfg.max_stream_num = 1;
    play_cfg.render_fps = TONGYI_PREVIEW_FPS;
    ESP_RETURN_ON_ERROR(esp_video_player_service_create(&play_cfg, &media->video_player),
                        TAG, "Create LCD player failed");
    esp_video_player_service_setup_t play_setup = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    play_setup.display_dev_name = ESP_BOARD_DEVICE_NAME_DISPLAY_LCD;
    /* Audio DAC already belongs to audio_player; do not reopen it at 48 kHz. */
    play_setup.audio_dev_name = NULL;
    ESP_RETURN_ON_ERROR(esp_video_player_service_apply_setup(media->video_player, &play_setup),
                        TAG, "Configure LCD player failed");

    esp_player_service_info_t info = {0};
    ESP_RETURN_ON_ERROR(esp_player_service_get_info(media->video_player, &info), TAG, "Get LCD render failed");
    ESP_RETURN_ON_FALSE(info.video_render != NULL, ESP_ERR_INVALID_STATE, TAG, "LCD has no render");
    return vision_open_preview_stream(media, (esp_video_render_handle_t)info.video_render);
}
#endif  /* CONFIG_TONGYI_APP_MODE_VISION */

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

static esp_err_t unlink_services(tongyi_app_media_t *media)
{
    if (media->llm == NULL) {
        return ESP_OK;
    }
    esp_err_t result = ESP_OK;
    esp_err_t ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                             ESP_SERVICE_BASE(media->audio_player), TONGYI_APP_MEDIA_STREAM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink LLM from audio player failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
    ret = esp_media_service_unlink(ESP_SERVICE_BASE(media->audio_capture), TONGYI_APP_MEDIA_STREAM,
                                   ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unlink audio capture from LLM failed: %s", esp_err_to_name(ret));
        record_first_error(&result, ret);
    }
    media->llm = NULL;
    return result;
}

static esp_err_t link_services(tongyi_app_media_t *media)
{
    esp_err_t ret = esp_media_service_link(ESP_SERVICE_BASE(media->audio_capture), TONGYI_APP_MEDIA_STREAM,
                                           ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(media->llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                     ESP_SERVICE_BASE(media->audio_player), TONGYI_APP_MEDIA_STREAM);
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Link 'media_graph': capture to LLM to player");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Link media graph failed: %s", esp_err_to_name(ret));
    (void)unlink_services(media);
    return ret;
}

esp_err_t tongyi_app_media_create(const esp_llm_tongyi_cfg_t *tongyi_cfg,
                                 const tongyi_app_media_cfg_t *cfg,
                                 tongyi_app_media_t **out_media)
{
    if (tongyi_cfg == NULL || cfg == NULL || out_media == NULL || *out_media != NULL) {
        ESP_LOGE(TAG, "Create failed: invalid argument or output is not NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->output_volume > 100) {
        ESP_LOGE(TAG, "Create failed: output volume is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    tongyi_app_media_t *media = calloc(1, sizeof(*media));
    if (media == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for media context");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_service_scheduler_set_cb(capture_scheduler_cb, NULL);
    if (ret == ESP_OK) {
        ret = init_board_audio();
    }
    if (ret == ESP_OK) {
        ret = create_audio_capture(tongyi_cfg, media);
    }
    if (ret == ESP_OK) {
        ret = create_audio_player(tongyi_cfg, cfg, media);
    }
#if CONFIG_TONGYI_APP_MODE_VISION
    if (ret == ESP_OK) {
        esp_err_t vision_ret = vision_open(media);
        if (vision_ret != ESP_OK) {
            vision_close(media);
            ESP_LOGW(TAG, "Vision disabled, audio continues: %s", esp_err_to_name(vision_ret));
        }
    }
#endif
    if (ret != ESP_OK) {
        (void)tongyi_app_media_destroy(&media);
        return ret;
    }

    *out_media = media;
    ESP_LOGI(TAG, "Create 'media_graph'");
    return ESP_OK;
}

esp_err_t tongyi_app_media_get_audio_capture(tongyi_app_media_t *media,
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

esp_err_t tongyi_app_media_capture_jpeg(tongyi_app_media_t *media, uint8_t **out_data, size_t *out_size)
{
    if (media == NULL || out_data == NULL || out_size == NULL) {
        ESP_LOGE(TAG, "Capture JPEG failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    *out_data = NULL;
    *out_size = 0;
#if !CONFIG_TONGYI_APP_MODE_VISION
    ESP_LOGE(TAG, "Capture JPEG failed: example was built as voice agent");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (media->video_capture == NULL) {
        ESP_LOGE(TAG, "Capture JPEG failed: camera is not running");
        return ESP_ERR_INVALID_STATE;
    }

    if (media->jpeg_lock != NULL &&
        xSemaphoreTake(media->jpeg_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (media->jpeg_cache != NULL && media->jpeg_cache_size > 0) {
            uint8_t *copy = malloc(media->jpeg_cache_size);
            if (copy == NULL) {
                xSemaphoreGive(media->jpeg_lock);
                ESP_LOGE(TAG, "Capture JPEG failed: no memory for %u bytes",
                         (unsigned)media->jpeg_cache_size);
                return ESP_ERR_NO_MEM;
            }
            memcpy(copy, media->jpeg_cache, media->jpeg_cache_size);
            *out_data = copy;
            *out_size = media->jpeg_cache_size;
            xSemaphoreGive(media->jpeg_lock);
            return ESP_OK;
        }
        xSemaphoreGive(media->jpeg_lock);
    }

    esp_media_provider_t jpeg = {0};
    ESP_RETURN_ON_ERROR(esp_media_service_get_provider(ESP_SERVICE_BASE(media->video_capture),
                                                       TONGYI_JPEG_STREAM, &jpeg),
                        TAG, "Get JPEG provider failed");

    esp_media_frame_t frame = { .type = ESP_MEDIA_TRACK_TYPE_VIDEO };
    esp_err_t ret = esp_media_provider_acquire_frame(&jpeg, &frame, 1000);
    if (ret != ESP_OK || frame.data == NULL || frame.size == 0) {
        ESP_LOGE(TAG, "Capture JPEG failed: acquire frame returned %s", esp_err_to_name(ret));
        return ret != ESP_OK ? ret : ESP_FAIL;
    }

    uint8_t *copy = malloc(frame.size);
    if (copy == NULL) {
        (void)esp_media_provider_release_frame(&jpeg, &frame);
        ESP_LOGE(TAG, "Capture JPEG failed: no memory for %u bytes", (unsigned)frame.size);
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, frame.data, frame.size);
    *out_data = copy;
    *out_size = frame.size;
    (void)esp_media_provider_release_frame(&jpeg, &frame);
    return ESP_OK;
#endif
}

esp_err_t tongyi_app_media_stop(tongyi_app_media_t *media)
{
    if (media == NULL) {
        ESP_LOGE(TAG, "Stop failed: media is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
#if CONFIG_TONGYI_APP_MODE_VISION
    vision_stop(media);
#endif
    try_stop(media->audio_capture, "audio capture", &result);
    try_stop(media->llm, "LLM service", &result);
    try_stop(media->audio_player, "audio player", &result);
    record_first_error(&result, unlink_services(media));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Stop 'media_graph'");
    }
    return result;
}

esp_err_t tongyi_app_media_start(tongyi_app_media_t *media, esp_llm_service_t *llm)
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
#if CONFIG_TONGYI_APP_MODE_VISION
    if (ret == ESP_OK) {
        ret = vision_start(media);
    }
#endif
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Start 'media_graph'");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Start media graph failed: %s", esp_err_to_name(ret));
    (void)tongyi_app_media_stop(media);
    return ret;
}

esp_err_t tongyi_app_media_destroy(tongyi_app_media_t **media)
{
    if (media == NULL || *media == NULL) {
        ESP_LOGE(TAG, "Destroy failed: media handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    tongyi_app_media_t *ctx = *media;
    esp_err_t result = tongyi_app_media_stop(ctx);

    if (ctx->audio_player != NULL) {
        record_first_error(&result, esp_player_service_destroy(ctx->audio_player));
        ctx->audio_player = NULL;
    }
#if CONFIG_TONGYI_APP_MODE_VISION
    vision_close(ctx);
#endif
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
