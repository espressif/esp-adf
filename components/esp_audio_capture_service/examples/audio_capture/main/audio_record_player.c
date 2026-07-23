/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_aac_dec.h"
#include "esp_ae_rate_cvt.h"
#include "esp_audio_dec.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_extractor.h"
#include "esp_g711_dec.h"
#include "esp_log.h"

#include "audio_record_player.h"
#include "settings.h"

#define EXTRACTOR_POOL_SIZE  (32 * 1024)
#define PCM_BUFFER_SIZE      (8 * 1024)

static const char *TAG = "RECORD_PLAYER";

extern const uint8_t music_aac_start[] asm("_binary_music_aac_start");
extern const uint8_t music_aac_end[] asm("_binary_music_aac_end");

typedef struct {
    FILE          *file;
    const uint8_t *data;
    uint32_t       size;
    uint32_t       pos;
} player_input_t;

typedef struct {
    esp_ae_rate_cvt_handle_t  rate_cvt;
    uint8_t                  *buffer;
    uint32_t                  buffer_size;
    uint8_t                  *stereo_buffer;
    uint32_t                  stereo_buffer_size;
    uint8_t                   src_channel;
    uint8_t                   bits_per_sample;
} player_output_t;

static dev_audio_codec_handles_t *s_dac;
static volatile bool s_music_running;
static volatile bool s_music_looping;
static TaskHandle_t s_music_task;

static int input_read(void *buffer, uint32_t size, void *ctx)
{
    player_input_t *input = ctx;
    if (input->file != NULL) {
        return fread(buffer, 1, size, input->file);
    }
    uint32_t remain = input->size - input->pos;
    uint32_t read_size = size < remain ? size : remain;
    memcpy(buffer, input->data + input->pos, read_size);
    input->pos += read_size;
    return read_size;
}

static int input_seek(uint32_t position, void *ctx)
{
    player_input_t *input = ctx;
    if (input->file != NULL) {
        return fseek(input->file, position, SEEK_SET);
    }
    if (position > input->size) {
        return -1;
    }
    input->pos = position;
    return 0;
}

static uint32_t input_size(void *ctx)
{
    player_input_t *input = ctx;
    if (input->file == NULL) {
        return input->size;
    }
    long position = ftell(input->file);
    fseek(input->file, 0, SEEK_END);
    long size = ftell(input->file);
    fseek(input->file, position, SEEK_SET);
    return size > 0 ? size : 0;
}

static esp_audio_type_t extractor_format_to_decoder(esp_extractor_format_t format)
{
    switch (format) {
        case ESP_EXTRACTOR_AUDIO_FORMAT_AAC:
            return ESP_AUDIO_TYPE_AAC;
        case ESP_EXTRACTOR_AUDIO_FORMAT_G711A:
            return ESP_AUDIO_TYPE_G711A;
        case ESP_EXTRACTOR_AUDIO_FORMAT_G711U:
            return ESP_AUDIO_TYPE_G711U;
        default:
            return ESP_AUDIO_TYPE_UNSUPPORT;
    }
}

static esp_err_t open_dac(uint8_t bits_per_sample)
{
    /* Always open stereo playback so the right slot feeds the board DAC-echo
       reference channel used by AEC (ADC layout RE/FC). */
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = AUDIO_RECORD_SAMPLE_RATE,
        .bits_per_sample = bits_per_sample,
        .channel = AUDIO_RECORD_PLAYBACK_CHANNELS,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1),
    };
    int ret = esp_codec_dev_open(s_dac->codec_dev, &sample_info);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open DAC: %d", ret);
        return ESP_FAIL;
    }
    esp_codec_dev_set_out_vol(s_dac->codec_dev, DEFAULT_VOL);
    return ESP_OK;
}

static esp_err_t player_output_open(const esp_extractor_audio_stream_info_t *info, player_output_t *output)
{
    esp_extractor_audio_stream_info_t normalized = *info;
    normalized.channel = normalized.channel ? normalized.channel : 1;
    normalized.bits_per_sample = normalized.bits_per_sample ? normalized.bits_per_sample : 16;
    output->src_channel = normalized.channel;
    output->bits_per_sample = normalized.bits_per_sample;
    if (normalized.sample_rate != AUDIO_RECORD_SAMPLE_RATE) {
        esp_ae_rate_cvt_cfg_t cfg = {
            .src_rate = normalized.sample_rate,
            .dest_rate = AUDIO_RECORD_SAMPLE_RATE,
            .channel = normalized.channel,
            .bits_per_sample = normalized.bits_per_sample,
            .complexity = 2,
            .perf_type = ESP_AE_RATE_CVT_PERF_TYPE_MEMORY,
        };
        if (esp_ae_rate_cvt_open(&cfg, &output->rate_cvt) != ESP_AE_ERR_OK) {
            ESP_LOGE(TAG, "Failed to create %" PRIu32 " Hz to %d Hz converter",
                     normalized.sample_rate, AUDIO_RECORD_SAMPLE_RATE);
            return ESP_FAIL;
        }
    }
    return open_dac(normalized.bits_per_sample);
}

static esp_err_t expand_to_stereo_and_write(player_output_t *output, const uint8_t *data, uint32_t size)
{
    uint32_t sample_bytes = output->bits_per_sample >> 3;
    if (sample_bytes == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (output->src_channel >= AUDIO_RECORD_PLAYBACK_CHANNELS) {
        return esp_codec_dev_write(s_dac->codec_dev, (void *)data, size) == ESP_CODEC_DEV_OK        ?
                                                                                             ESP_OK : ESP_FAIL;
    }

    uint32_t frames = size / (sample_bytes * output->src_channel);
    uint32_t stereo_size = frames * sample_bytes * AUDIO_RECORD_PLAYBACK_CHANNELS;
    if (stereo_size > output->stereo_buffer_size) {
        uint8_t *new_buffer = realloc(output->stereo_buffer, stereo_size);
        if (new_buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
        output->stereo_buffer = new_buffer;
        output->stereo_buffer_size = stereo_size;
    }

    /* Duplicate mono (or first channel) into L/R so the right slot carries REF. */
    if (sample_bytes == 2) {
        const int16_t *in = (const int16_t *)data;
        int16_t *out = (int16_t *)output->stereo_buffer;
        for (uint32_t i = 0; i < frames; i++) {
            int16_t sample = in[i * output->src_channel];
            out[2 * i] = sample;
            out[2 * i + 1] = sample;
        }
    } else {
        for (uint32_t i = 0; i < frames; i++) {
            const uint8_t *src = data + i * sample_bytes * output->src_channel;
            uint8_t *dst = output->stereo_buffer + i * sample_bytes * AUDIO_RECORD_PLAYBACK_CHANNELS;
            memcpy(dst, src, sample_bytes);
            memcpy(dst + sample_bytes, src, sample_bytes);
        }
    }
    return esp_codec_dev_write(s_dac->codec_dev, output->stereo_buffer, stereo_size) == ESP_CODEC_DEV_OK ?
                                                                                                         ESP_OK
                                                                                                         : ESP_FAIL;
}

static esp_err_t player_output_write(player_output_t *output, uint8_t *data, uint32_t size)
{
    if (output->rate_cvt == NULL) {
        return expand_to_stereo_and_write(output, data, size);
    }
    uint32_t sample_bytes = output->src_channel * (output->bits_per_sample >> 3);
    uint32_t input_samples = size / sample_bytes;
    uint32_t output_samples = 0;
    if (esp_ae_rate_cvt_get_max_out_sample_num(output->rate_cvt, input_samples,
                                               &output_samples) != ESP_AE_ERR_OK) {
        return ESP_FAIL;
    }
    uint32_t required_size = output_samples * sample_bytes;
    if (required_size > output->buffer_size) {
        uint8_t *new_buffer = realloc(output->buffer, required_size);
        if (new_buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
        output->buffer = new_buffer;
        output->buffer_size = required_size;
    }
    if (esp_ae_rate_cvt_process(output->rate_cvt, (esp_ae_sample_t)data, input_samples,
                                (esp_ae_sample_t)output->buffer, &output_samples) != ESP_AE_ERR_OK) {
        return ESP_FAIL;
    }
    uint32_t output_size = output_samples * sample_bytes;
    return expand_to_stereo_and_write(output, output->buffer, output_size);
}

static void player_output_close(player_output_t *output)
{
    esp_codec_dev_close(s_dac->codec_dev);
    if (output->rate_cvt != NULL) {
        esp_ae_rate_cvt_close(output->rate_cvt);
    }
    free(output->buffer);
    free(output->stereo_buffer);
}

static esp_err_t play_extracted_frames(esp_extractor_handle_t extractor,
                                       const esp_extractor_stream_info_t *stream_info)
{
    bool pcm = stream_info->audio_info.format == ESP_EXTRACTOR_AUDIO_FORMAT_PCM;
    esp_audio_dec_handle_t decoder = NULL;
    uint8_t *pcm_buffer = NULL;
    player_output_t output = {0};
    esp_err_t result = ESP_OK;

    if (!pcm) {
        esp_audio_type_t type = extractor_format_to_decoder(stream_info->audio_info.format);
        if (type == ESP_AUDIO_TYPE_UNSUPPORT) {
            ESP_LOGE(TAG, "Unsupported verification codec: 0x%" PRIx32,
                     (uint32_t)stream_info->audio_info.format);
            return ESP_ERR_NOT_SUPPORTED;
        }
        esp_aac_dec_cfg_t codec_cfg = {0};
        esp_g711_dec_cfg_t g711_cfg = {
            .channel = stream_info->audio_info.channel,
        };
        esp_audio_dec_cfg_t cfg = {
            .type = type,
            .cfg = type == ESP_AUDIO_TYPE_AAC ? (void *)&codec_cfg : (void *)&g711_cfg,
            .cfg_sz = type == ESP_AUDIO_TYPE_AAC ? sizeof(codec_cfg) : sizeof(g711_cfg),
        };
        if (esp_audio_dec_open(&cfg, &decoder) != ESP_AUDIO_ERR_OK) {
            return ESP_FAIL;
        }
        pcm_buffer = malloc(PCM_BUFFER_SIZE);
        if (pcm_buffer == NULL) {
            esp_audio_dec_close(decoder);
            return ESP_ERR_NO_MEM;
        }
    }

    result = player_output_open(&stream_info->audio_info, &output);
    if (result != ESP_OK) {
        goto cleanup;
    }
    while (s_music_task == NULL || s_music_running) {
        esp_extractor_frame_info_t frame = {0};
        esp_extractor_err_t extract_ret = esp_extractor_read_frame(extractor, &frame);
        if (extract_ret == ESP_EXTRACTOR_ERR_EOS) {
            if (!s_music_looping) {
                break;
            }
            ESP_LOGI(TAG, "Music loop");
            extract_ret = esp_extractor_seek(extractor, 0);
        }
        if (extract_ret != ESP_EXTRACTOR_ERR_OK) {
            result = ESP_FAIL;
            break;
        }
        if (frame.stream_type != ESP_EXTRACTOR_STREAM_TYPE_AUDIO) {
            esp_extractor_release_frame(extractor, &frame);
            continue;
        }
        if (pcm) {
            if (player_output_write(&output, frame.frame_buffer, frame.frame_size) != ESP_OK) {
                result = ESP_FAIL;
            }
        } else {
            esp_audio_dec_in_raw_t raw = {
                .buffer = frame.frame_buffer,
                .len = frame.frame_size,
            };
            while (raw.len > 0) {
                esp_audio_dec_out_frame_t out = {
                    .buffer = pcm_buffer,
                    .len = PCM_BUFFER_SIZE,
                };
                esp_audio_err_t dec_ret = esp_audio_dec_process(decoder, &raw, &out);
                if (dec_ret != ESP_AUDIO_ERR_OK) {
                    result = ESP_FAIL;
                    break;
                }
                if (out.decoded_size > 0 && player_output_write(&output, out.buffer,
                                                                out.decoded_size) != ESP_OK) {
                    result = ESP_FAIL;
                    break;
                }
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;
            }
        }
        esp_extractor_release_frame(extractor, &frame);
        if (result != ESP_OK) {
            break;
        }
    }

cleanup:
    player_output_close(&output);
    if (decoder != NULL) {
        esp_audio_dec_close(decoder);
    }
    free(pcm_buffer);
    return result;
}

static esp_err_t play_input(player_input_t *input, esp_extractor_type_t type)
{
    esp_extractor_config_t cfg = {
        .type = type,
        .extract_mask = ESP_EXTRACT_MASK_AUDIO,
        .in_read_cb = input_read,
        .in_seek_cb = input_seek,
        .in_size_cb = input_size,
        .in_ctx = input,
        .out_pool_size = EXTRACTOR_POOL_SIZE,
        .out_align = 16,
    };
    esp_extractor_handle_t extractor = NULL;
    if (esp_extractor_open(&cfg, &extractor) != ESP_EXTRACTOR_ERR_OK) {
        return ESP_FAIL;
    }
    esp_err_t result = ESP_FAIL;
    do {
        if (esp_extractor_parse_stream(extractor) != ESP_EXTRACTOR_ERR_OK) {
            break;
        }
        esp_extractor_stream_info_t stream_info = {0};
        if (esp_extractor_get_stream_info(extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0,
                                          &stream_info) != ESP_EXTRACTOR_ERR_OK) {
            break;
        }
        ESP_LOGI(TAG, "Stream info codec: %d channels: %d sample rate: %d bits per sample: %d",
                 stream_info.audio_info.format, stream_info.audio_info.channel, stream_info.audio_info.sample_rate, stream_info.audio_info.bits_per_sample);

        result = play_extracted_frames(extractor, &stream_info);
    } while (0);
    esp_extractor_close(extractor);
    return result;
}

esp_err_t audio_record_player_init(void)
{
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&s_dac);
    if (ret != ESP_OK || s_dac == NULL || s_dac->codec_dev == NULL) {
        s_dac = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static void music_task(void *ctx)
{
    player_input_t input = {
        .data = music_aac_start,
        .size = music_aac_end - music_aac_start,
    };
    esp_err_t ret = play_input(&input, ESP_EXTRACTOR_TYPE_AAC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Background music playback failed: %s", esp_err_to_name(ret));
    }
    s_music_running = false;
    s_music_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_record_player_start_music(void)
{
    if (s_dac == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_music_task != NULL) {
        return ESP_OK;
    }
    s_music_running = true;
    s_music_looping = true;
    if (xTaskCreate(music_task, "aec_music", AUDIO_RECORD_PLAYER_STACK_SIZE, NULL, 5,
                    &s_music_task) != pdPASS) {
        s_music_running = false;
        s_music_looping = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void audio_record_player_stop_music(void)
{
    s_music_running = false;
    while (s_music_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t audio_record_player_play_file(const char *path)
{
    if (s_dac == NULL || path == NULL || s_music_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    player_input_t input = {
        .file = file,
    };
    const char *extension = strrchr(path, '.');
    esp_extractor_type_t type = ESP_EXTRACTOR_TYPE_NONE;
    if (extension != NULL && strcasecmp(extension, ".wav") == 0) {
        type = ESP_EXTRACTOR_TYPE_WAV;
    } else if (extension != NULL && strcasecmp(extension, ".mp4") == 0) {
        type = ESP_EXTRACTOR_TYPE_MP4;
    }
    s_music_looping = false;
    esp_err_t ret = play_input(&input, type);
    fclose(file);
    return ret;
}
