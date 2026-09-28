/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Agent audio profile (same mapping as multimodal_assistant_app):
 *   OPUS — app encodes/decodes OPUS; SDK does not transcode;
 *           AudioIn = opus, rtc_ac/AudioOut = opus.
 *   PCM  — app sends/plays PCM; SDK encodes/decodes G.722;
 *           AudioIn = pcm, rtc_ac/AudioOut = g722.
 */

#include <string.h>

#include "sdkconfig.h"

#include "esp_log.h"
#include "esp_llm_baidu_internal.h"

static const char *TAG = "ESP_LLM_BAIDU";

static bool baidu_audio_is_opus(const esp_llm_baidu_cfg_t *cfg)
{
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        return baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio.codec == ESP_FOURCC_OPUS;
    }
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        return baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio.codec == ESP_FOURCC_OPUS;
    }
    return false;
}

static esp_err_t baidu_validate_audio(const esp_media_audio_info_t *audio, uint32_t expected_rate)
{
    if ((audio->codec != ESP_FOURCC_PCM && audio->codec != ESP_FOURCC_OPUS) ||
        audio->channel != 1U || audio->bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (expected_rate != 0 && audio->sample_rate != expected_rate) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t baidu_validate_video(const esp_media_video_info_t *video)
{
    if ((video->codec != ESP_FOURCC_H264 && video->codec != ESP_FOURCC_MJPG) ||
        video->width == 0 || video->height == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t baidu_validate_streams(const esp_llm_baidu_cfg_t *cfg,
                                 const esp_llm_baidu_resolved_scene_t *scene)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Validate streams failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t expected_rate = cfg->mode == ESP_LLM_BAIDU_MODE_INTERCOM ?
                             BAIDU_INTERCOM_PCM_RATE : BAIDU_TTS_PCM_RATE;
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        esp_err_t ret = baidu_validate_audio(
            &baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio, expected_rate);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Validate streams failed: audio uplink format is not supported");
            return ret;
        }
    }
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        esp_err_t ret = baidu_validate_audio(
            &baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio, expected_rate);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Validate streams failed: audio downlink format is not supported");
            return ret;
        }
    }
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)) {
        if (cfg->mode == ESP_LLM_BAIDU_MODE_AGENT &&
            (scene == NULL || !scene->enable_visual ||
             cfg->vision.vision_mode == ESP_LLM_BAIDU_VISION_MODE_NONE)) {
            ESP_LOGE(TAG, "Validate streams failed: video uplink is not enabled for this scene");
            return ESP_ERR_NOT_SUPPORTED;
        }
        esp_err_t ret = baidu_validate_video(
            &baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Validate streams failed: video uplink format is not supported");
            return ret;
        }
    }
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        if (cfg->mode == ESP_LLM_BAIDU_MODE_AGENT &&
            (scene == NULL || !scene->enable_image_generate)) {
            ESP_LOGE(TAG, "Validate streams failed: video downlink is not enabled for this scene");
            return ESP_ERR_NOT_SUPPORTED;
        }
        esp_err_t ret = baidu_validate_video(
            &baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Validate streams failed: video downlink format is not supported");
            return ret;
        }
    }
    return ESP_OK;
}

void baidu_resolve_scene(esp_llm_baidu_scene_type_t scene_type,
                         esp_llm_baidu_resolved_scene_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));

    switch (scene_type) {
        case ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING:
            out->scene_name = "ImageUnderstanding";
            out->enable_visual = true;
            out->enable_image_generate = false;
            break;
        case ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION:
            out->scene_name = "ImageGeneration";
            out->enable_visual = false;
            out->enable_image_generate = true;
            break;
        case ESP_LLM_BAIDU_SCENE_MULTI_MODAL:
            out->scene_name = "MultiModalAgent";
            out->enable_visual = true;
            out->enable_image_generate = true;
            break;
        case ESP_LLM_BAIDU_SCENE_VOICE_CHAT:
        default:
            out->scene_name = ESP_LLM_BAIDU_DEFAULT_WORKFLOW;
            out->enable_visual = false;
            out->enable_image_generate = false;
            break;
    }
}

bool baidu_scene_needs_camera(const esp_llm_baidu_resolved_scene_t *scene)
{
    if (scene == NULL) {
        return false;
    }
    return scene->enable_visual;
}

const char *baidu_agent_sdk_uplink_codec(const esp_llm_baidu_cfg_t *cfg)
{
    /* Bytes passed to send_audio: OPUS frames, or PCM (SDK then encodes G.722). */
    return baidu_audio_is_opus(cfg) ? ESP_LLM_BAIDU_CODEC_OPUS : ESP_LLM_BAIDU_CODEC_PCM;
}

const char *baidu_agent_sdk_rtc_ac(const esp_llm_baidu_cfg_t *cfg)
{
    /* Wire codec: OPUS passthrough, or G.722 inside the Baidu lib. */
    return baidu_audio_is_opus(cfg) ? ESP_LLM_BAIDU_CODEC_OPUS : ESP_LLM_BAIDU_CODEC_G722;
}

bool baidu_rtc_ac_is_opus(const char *rtc_ac)
{
    return rtc_ac != NULL && strstr(rtc_ac, ESP_LLM_BAIDU_CODEC_OPUS) != NULL;
}

const char *baidu_sdk_video_codec(const esp_llm_baidu_cfg_t *cfg)
{
    esp_media_codec_fourcc_t codec = ESP_FOURCC_MJPG;
    if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)) {
        codec = baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video.codec;
    } else if (baidu_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        codec = baidu_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.codec;
    }
    return codec == ESP_FOURCC_H264 ? ESP_LLM_BAIDU_CODEC_H264 : ESP_LLM_BAIDU_CODEC_JPEG;
}

void baidu_apply_downlink_audio_profile(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        const esp_media_audio_info_t *audio =
            &baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio;
        ctx->downlink_codec = audio->codec;
        ctx->downlink_rate = audio->sample_rate;
        ctx->downlink_channel = audio->channel != 0 ? audio->channel : 1U;
        ctx->downlink_bits = audio->bits_per_sample >= 8U ?
                             (uint8_t)audio->bits_per_sample : 16U;
    }
    if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        ctx->downlink_fps = baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.fps;
    }
}
