/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_fourcc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "media_dummy_priv.h"

static const char *TAG = "DUMMY_PATTERN";

extern const uint8_t _binary_s_aac_start[] asm("_binary_s_aac_start");
extern const uint8_t _binary_s_aac_end[] asm("_binary_s_aac_end");
extern const uint8_t _binary_s_opus_start[] asm("_binary_s_opus_start");
extern const uint8_t _binary_s_opus_end[] asm("_binary_s_opus_end");
extern const uint8_t _binary_s_mp3_start[] asm("_binary_s_mp3_start");
extern const uint8_t _binary_s_mp3_end[] asm("_binary_s_mp3_end");
extern const uint8_t _binary_s_h264_start[] asm("_binary_s_h264_start");
extern const uint8_t _binary_s_h264_end[] asm("_binary_s_h264_end");
extern const uint8_t _binary_s_mjpeg_start[] asm("_binary_s_mjpeg_start");
extern const uint8_t _binary_s_mjpeg_end[] asm("_binary_s_mjpeg_end");

static bool field_ok(uint32_t got, uint32_t expect)
{
    return got == 0 || got == expect || expect == 0;
}

static esp_err_t match_or_fill_audio(esp_media_audio_info_t *dst, const esp_media_audio_info_t *req,
                                     const esp_media_audio_info_t *def)
{
    if (!field_ok(req->sample_rate, def->sample_rate) ||
        !field_ok(req->bits_per_sample, def->bits_per_sample) ||
        !field_ok(req->channel, def->channel) ||
        !field_ok(req->bitrate, def->bitrate)) {
        return ESP_ERR_INVALID_ARG;
    }
    *dst = *def;
    dst->codec = def->codec;
    if (req->sample_rate) {
        dst->sample_rate = req->sample_rate;
    }
    if (req->bits_per_sample) {
        dst->bits_per_sample = req->bits_per_sample;
    }
    if (req->channel) {
        dst->channel = req->channel;
    }
    if (req->bitrate) {
        dst->bitrate = req->bitrate;
    }
    return ESP_OK;
}

static esp_err_t match_or_fill_video(esp_media_video_info_t *dst, const esp_media_video_info_t *req,
                                     const esp_media_video_info_t *def)
{
    if (!field_ok(req->width, def->width) ||
        !field_ok(req->height, def->height) ||
        !field_ok(req->fps, def->fps) ||
        !field_ok(req->bitrate, def->bitrate)) {
        return ESP_ERR_INVALID_ARG;
    }
    *dst = *def;
    if (req->width) {
        dst->width = req->width;
    }
    if (req->height) {
        dst->height = req->height;
    }
    if (req->fps) {
        dst->fps = req->fps;
    }
    if (req->bitrate) {
        dst->bitrate = req->bitrate;
    }
    return ESP_OK;
}

static void set_encoded(media_dummy_pattern_t *out, const uint8_t *start, const uint8_t *end,
                        const esp_media_track_info_t *info, uint32_t loop_frames,
                        uint32_t frame_duration_ms)
{
    out->data = start;
    out->size = (size_t)(end - start);
    out->loop_frames = loop_frames;
    out->frame_duration_ms = frame_duration_ms;
    out->track_info = *info;
    out->encoded = true;
    out->owned = false;
}

uint32_t media_dummy_pattern_frame_count(const media_dummy_pattern_t *pattern)
{
    if (pattern == NULL || pattern->data == NULL || pattern->size == 0) {
        return 0;
    }
    if (!pattern->encoded) {
        return pattern->frame_size ? pattern->size / pattern->frame_size : 1;
    }
    uint32_t count = 0;
    size_t off = 0;
    while (off + 2 <= pattern->size) {
        uint16_t len = (uint16_t)pattern->data[off] | ((uint16_t)pattern->data[off + 1] << 8);
        off += 2;
        if (len == 0 || off + len > pattern->size) {
            break;
        }
        off += len;
        count++;
    }
    return count;
}

size_t media_dummy_pattern_max_frame_size(const media_dummy_pattern_t *pattern)
{
    if (pattern == NULL || pattern->data == NULL) {
        return 0;
    }
    if (!pattern->encoded) {
        return pattern->frame_size ? pattern->frame_size : pattern->size;
    }
    size_t max_size = 0;
    size_t off = 0;
    while (off + 2 <= pattern->size) {
        uint16_t len = (uint16_t)pattern->data[off] | ((uint16_t)pattern->data[off + 1] << 8);
        off += 2;
        if (len == 0 || off + len > pattern->size) {
            break;
        }
        if (len > max_size) {
            max_size = len;
        }
        off += len;
    }
    return max_size;
}

static uint32_t audio_samples_duration_ms(uint32_t samples, uint32_t sample_rate)
{
    if (samples == 0) {
        return 0;
    }
    if (sample_rate == 0) {
        sample_rate = DUMMY_DEFAULT_AUDIO_SR;
    }
    return (uint32_t)((1000ULL * samples) / sample_rate);
}

static uint32_t video_fps_duration_ms(uint32_t fps)
{
    if (fps == 0) {
        fps = DUMMY_DEFAULT_VIDEO_FPS;
    }
    return 1000U / fps;
}

uint32_t media_dummy_pattern_frame_duration_ms(const media_dummy_pattern_t *pattern)
{
    if (pattern == NULL) {
        return 0;
    }
    if (pattern->frame_duration_ms) {
        return pattern->frame_duration_ms;
    }

    if (pattern->track_info.type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        const esp_media_audio_info_t *audio = &pattern->track_info.info.audio;
        uint32_t sample_rate = audio->sample_rate ? audio->sample_rate : DUMMY_DEFAULT_AUDIO_SR;
        switch (audio->codec) {
            case ESP_FOURCC_OPUS:
                /* s.opus / ffmpeg default packetization is 20 ms. */
                return DUMMY_OPUS_FRAME_MS;
            case ESP_FOURCC_MP3:
                /* s.mp3 is MPEG-2 Layer III: 576 samples/frame. */
                return audio_samples_duration_ms(DUMMY_MP3_MPEG2_SAMPLES, sample_rate);
            case ESP_FOURCC_AAC:
                return audio_samples_duration_ms(DUMMY_AAC_SAMPLES_PER_FRAME, sample_rate);
            case ESP_FOURCC_PCM:
            case ESP_FOURCC_PCM_S16:
                if (pattern->frame_size && audio->channel && audio->bits_per_sample >= 8) {
                    uint32_t bytes_per_sample = audio->channel * (audio->bits_per_sample / 8);
                    if (bytes_per_sample) {
                        return audio_samples_duration_ms((uint32_t)(pattern->frame_size / bytes_per_sample),
                                                         sample_rate);
                    }
                }
                return DUMMY_PCM_FRAME_MS;
            default:
                return audio_samples_duration_ms(DUMMY_AAC_SAMPLES_PER_FRAME, sample_rate);
        }
    }

    if (pattern->track_info.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return video_fps_duration_ms(pattern->track_info.info.video.fps);
    }
    return 0;
}

esp_err_t media_dummy_pattern_get_frame(media_dummy_pattern_t *pattern, uint32_t index,
                                        const uint8_t **data, size_t *size, bool *key)
{
    if (pattern == NULL || data == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (pattern->data == NULL || pattern->size == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!pattern->encoded) {
        size_t frame_size = pattern->frame_size ? pattern->frame_size : pattern->size;
        uint32_t count = pattern->frame_size ? pattern->size / pattern->frame_size : 1;
        *data = pattern->data + (index % count) * frame_size;
        *size = frame_size;
        if (key) {
            *key = true;
        }
        return ESP_OK;
    }

    uint32_t count = media_dummy_pattern_frame_count(pattern);
    if (count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    uint32_t loop_frames = pattern->loop_frames ? pattern->loop_frames : count;
    uint32_t frame_idx = index % loop_frames;
    /* Compact audio assets store signal frames followed by one mute frame.
     * Hold that final mute frame until the 1-second loop boundary. */
    if (frame_idx >= count) {
        frame_idx = count - 1;
    }
    size_t off = 0;
    uint32_t cur = 0;
    while (off + 2 <= pattern->size) {
        uint16_t len = (uint16_t)pattern->data[off] | ((uint16_t)pattern->data[off + 1] << 8);
        off += 2;
        if (len == 0 || off + len > pattern->size) {
            break;
        }
        if (cur == frame_idx) {
            *data = pattern->data + off;
            *size = len;
            if (key) {
                *key = (frame_idx == 0);
            }
            return ESP_OK;
        }
        off += len;
        cur++;
    }
    return ESP_ERR_NOT_FOUND;
}

static void write_pop(int16_t *dst, uint32_t n, uint32_t sr, float freq, float amp, uint8_t channels)
{
    for (uint32_t i = 0; i < n; i++) {
        /* Sharp attack + exponential decay (~image sync.wav style). */
        float env = expf(-5.5f * (float)i / (float)((n > 1) ? (n - 1) : 1));
        float sample = amp * env * sinf(2.0f * (float)M_PI * freq * (float)i / (float)sr);
        int16_t v = (int16_t)(sample * 32767.0f);
        for (uint8_t ch = 0; ch < channels; ch++) {
            dst[i * channels + ch] = v;
        }
    }
}

static esp_err_t build_pcm_tone(const esp_media_track_info_t *req, media_dummy_pattern_t *out)
{
    esp_media_audio_info_t def = {
        .codec = ESP_FOURCC_PCM,
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channel = 1,
        .bitrate = 0,
    };
    esp_media_audio_info_t audio = {0};
    ESP_RETURN_ON_ERROR(match_or_fill_audio(&audio, &req->info.audio, &def), TAG, "pcm meta");
    if (audio.bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio.sample_rate == 0 || audio.channel == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /**
     * sync.wav style: 1 s period, audible content only in the first ~100 ms
     * (single ~1 kHz decaying pop), remaining samples are silence.
     * Slice into ~20 ms frames so capture read_frame buffers stay small.
     */
    const uint32_t period_ms = 1000;
    const uint32_t active_ms = 100;
    const uint32_t frame_ms = DUMMY_PCM_FRAME_MS;
    uint32_t period_samples = audio.sample_rate * period_ms / 1000;
    uint32_t active_samples = audio.sample_rate * active_ms / 1000;
    uint32_t frame_samples = audio.sample_rate * frame_ms / 1000;
    if (period_samples == 0 || frame_samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Align period to an integer number of frames for seamless looping. */
    uint32_t frame_count = (period_samples + frame_samples - 1) / frame_samples;
    period_samples = frame_count * frame_samples;
    if (active_samples > period_samples) {
        active_samples = period_samples;
    }

    size_t bytes = (size_t)period_samples * audio.channel * (audio.bits_per_sample / 8);
    size_t frame_bytes = (size_t)frame_samples * audio.channel * (audio.bits_per_sample / 8);
    int16_t *buf = calloc(1, bytes);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    /* One decaying pop in the first 100 ms; rest stays 0 from calloc. */
    write_pop(buf, active_samples, audio.sample_rate, 1000.0f, 0.45f, audio.channel);

    out->data = (const uint8_t *)buf;
    out->size = bytes;
    out->frame_size = frame_bytes;
    out->frame_duration_ms = audio_samples_duration_ms(frame_samples, audio.sample_rate);
    out->encoded = false;
    out->owned = true;
    out->track_info = *req;
    out->track_info.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
    out->track_info.info.audio = audio;
    return ESP_OK;
}

static size_t raw_video_frame_size(esp_media_codec_fourcc_t codec, uint16_t w, uint16_t h)
{
    switch (codec) {
        case ESP_FOURCC_RGB16:
        case ESP_FOURCC_RGB16_BE:
            return (size_t)w * h * 2;
        case ESP_FOURCC_RGB24:
            return (size_t)w * h * 3;
        case ESP_FOURCC_YUV420P:
            return (size_t)w * h * 3 / 2;
        default:
            return 0;
    }
}

static void fill_rgb16(uint16_t *dst, uint16_t w, uint16_t h, bool red)
{
    uint16_t color = red ? 0xF800 : 0xFFFF;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        /* Simple vertical color bar variation on white/red base. */
        uint16_t x = (uint16_t)(i % w);
        uint16_t bar = (uint16_t)((x * 8) / (w ? w : 1));
        if (!red) {
            static const uint16_t bars[] = {
                0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000
            };
            dst[i] = bars[bar & 7];
        } else {
            dst[i] = color;
        }
    }
}

static void fill_rgb24(uint8_t *dst, uint16_t w, uint16_t h, bool red)
{
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            size_t o = ((size_t)y * w + x) * 3;
            if (red) {
                dst[o] = 255;
                dst[o + 1] = 0;
                dst[o + 2] = 0;
            } else {
                uint8_t bar = (uint8_t)((x * 8) / (w ? w : 1));
                static const uint8_t bars[8][3] = {
                    {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0}, {255, 0, 255}, {255, 0, 0}, {0, 0, 255}, {0, 0, 0},
                };
                dst[o] = bars[bar & 7][0];
                dst[o + 1] = bars[bar & 7][1];
                dst[o + 2] = bars[bar & 7][2];
            }
        }
    }
}

static void fill_yuv420(uint8_t *dst, uint16_t w, uint16_t h, bool red)
{
    size_t y_size = (size_t)w * h;
    uint8_t *y = dst;
    uint8_t *u = dst + y_size;
    uint8_t *v = u + y_size / 4;
    memset(y, red ? 76 : 235, y_size);
    memset(u, red ? 84 : 128, y_size / 4);
    memset(v, red ? 255 : 128, y_size / 4);
    if (!red) {
        for (uint16_t yy = 0; yy < h; yy++) {
            for (uint16_t xx = 0; xx < w; xx++) {
                uint8_t bar = (uint8_t)((xx * 8) / (w ? w : 1));
                y[yy * w + xx] = (uint8_t)(40 + bar * 24);
            }
        }
    }
}

static esp_err_t build_raw_video(const esp_media_track_info_t *req, media_dummy_pattern_t *out)
{
    esp_media_video_info_t def = {
        .codec = req->info.video.codec ? req->info.video.codec : ESP_FOURCC_RGB16,
        .fps = DUMMY_DEFAULT_VIDEO_FPS,
        .width = 320,
        .height = 240,
        .bitrate = 0,
    };
     printf("request %dx%d fps:%d\n", req->info.video.width, req->info.video.height, req->info.video.fps);
    if (req->info.video.width > 0 && req->info.video.height > 0) {
        def.width = req->info.video.width;
        def.height = req->info.video.height;
    }
    if (req->info.video.fps > 0) {
        def.fps = req->info.video.fps;
    }
    esp_media_video_info_t video = {0};
    ESP_RETURN_ON_ERROR(match_or_fill_video(&video, &req->info.video, &def), TAG, "raw video meta");
    if (req->info.video.codec == 0) {
        video.codec = ESP_FOURCC_RGB16;
    } else {
        video.codec = req->info.video.codec;
    }
    size_t frame_size = raw_video_frame_size(video.codec, video.width, video.height);
    if (frame_size == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Two aligned framebuffers: color-bar then solid red. */
    size_t total = frame_size * 2;
    uint8_t *buf = heap_caps_aligned_calloc(DUMMY_FRAME_ALIGN, 1, total, MALLOC_CAP_DEFAULT);
    if (buf == NULL) {
        buf = calloc(1, total);
    }
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (video.codec == ESP_FOURCC_RGB16) {
        fill_rgb16((uint16_t *)buf, video.width, video.height, false);
        fill_rgb16((uint16_t *)(buf + frame_size), video.width, video.height, true);
    } else if (video.codec == ESP_FOURCC_RGB24) {
        fill_rgb24(buf, video.width, video.height, false);
        fill_rgb24(buf + frame_size, video.width, video.height, true);
    } else {
        fill_yuv420(buf, video.width, video.height, false);
        fill_yuv420(buf + frame_size, video.width, video.height, true);
    }

    out->data = buf;
    out->size = total;
    out->frame_size = frame_size;
    out->frame_duration_ms = video_fps_duration_ms(video.fps);
    out->encoded = false;
    out->owned = true;
    out->track_info = *req;
    out->track_info.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
    out->track_info.info.video = video;
    return ESP_OK;
}

esp_err_t media_dummy_pattern_build(const esp_media_track_info_t *req, media_dummy_pattern_t *out)
{
    if (req == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    if (req->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        esp_media_codec_fourcc_t codec = req->info.audio.codec;
        if (codec == ESP_FOURCC_PCM || codec == ESP_FOURCC_PCM_S16) {
            return build_pcm_tone(req, out);
        }

        esp_media_track_info_t info = *req;
        info.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
        if (codec == ESP_FOURCC_AAC) {
            esp_media_audio_info_t def = {
                .codec = ESP_FOURCC_AAC, .sample_rate = 16000, .bits_per_sample = 16, .channel = 1, .bitrate = 32000
            };
            ESP_RETURN_ON_ERROR(match_or_fill_audio(&info.info.audio, &req->info.audio, &def), TAG, "aac");
            /* AAC-LC: 1024 samples/frame; loop_frames ~= 1 s at pattern sample rate. */
            uint32_t frame_ms = audio_samples_duration_ms(DUMMY_AAC_SAMPLES_PER_FRAME, info.info.audio.sample_rate);
            uint32_t loop_frames = frame_ms ? (1000U / frame_ms) : 16;
            set_encoded(out, _binary_s_aac_start, _binary_s_aac_end, &info, loop_frames, frame_ms);
            return ESP_OK;
        }
        if (codec == ESP_FOURCC_OPUS) {
            esp_media_audio_info_t def = {
                .codec = ESP_FOURCC_OPUS, .sample_rate = 48000, .bits_per_sample = 16, .channel = 1, .bitrate = 16000
            };
            ESP_RETURN_ON_ERROR(match_or_fill_audio(&info.info.audio, &req->info.audio, &def), TAG, "opus");
            /* s.opus: 20 ms packets; five signal + mute hold for ~1 s. */
            uint32_t loop_frames = 1000U / DUMMY_OPUS_FRAME_MS;
            set_encoded(out, _binary_s_opus_start, _binary_s_opus_end, &info, loop_frames, DUMMY_OPUS_FRAME_MS);
            return ESP_OK;
        }
        if (codec == ESP_FOURCC_MP3) {
            esp_media_audio_info_t def = {
                .codec = ESP_FOURCC_MP3, .sample_rate = 16000, .bits_per_sample = 16, .channel = 1, .bitrate = 32000
            };
            ESP_RETURN_ON_ERROR(match_or_fill_audio(&info.info.audio, &req->info.audio, &def), TAG, "mp3");
            /* s.mp3 MPEG-2 Layer III: 576 samples/frame. */
            uint32_t frame_ms = audio_samples_duration_ms(DUMMY_MP3_MPEG2_SAMPLES, info.info.audio.sample_rate);
            uint32_t loop_frames = frame_ms ? (1000U / frame_ms) : 28;
            set_encoded(out, _binary_s_mp3_start, _binary_s_mp3_end, &info, loop_frames, frame_ms);
            return ESP_OK;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (req->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        esp_media_codec_fourcc_t codec = req->info.video.codec;
        esp_media_track_info_t info = *req;
        info.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
        if (codec == ESP_FOURCC_H264) {
            esp_media_video_info_t def = {
                .codec = ESP_FOURCC_H264, .width = 320, .height = 240, .fps = 15, .bitrate = 0
            };
            ESP_RETURN_ON_ERROR(match_or_fill_video(&info.info.video, &req->info.video, &def), TAG, "h264");
            /* gen_patterns.py encodes s.h264 at 15 fps. */
            set_encoded(out, _binary_s_h264_start, _binary_s_h264_end, &info, 0,
                        video_fps_duration_ms(info.info.video.fps));
            return ESP_OK;
        }
        if (codec == ESP_FOURCC_MJPG) {
            esp_media_video_info_t def = {
                .codec = ESP_FOURCC_MJPG, .width = 640, .height = 480, .fps = 10, .bitrate = 0
            };
            ESP_RETURN_ON_ERROR(match_or_fill_video(&info.info.video, &req->info.video, &def), TAG, "mjpeg");
            /* gen_patterns.py encodes s.mjpeg at 10 fps. */
            set_encoded(out, _binary_s_mjpeg_start, _binary_s_mjpeg_end, &info, 0,
                        video_fps_duration_ms(info.info.video.fps));
            return ESP_OK;
        }
        return build_raw_video(req, out);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

void media_dummy_pattern_release(media_dummy_pattern_t *pattern)
{
    if (pattern == NULL) {
        return;
    }
    if (pattern->owned && pattern->data != NULL) {
        free((void *)pattern->data);
    }
    memset(pattern, 0, sizeof(*pattern));
}
