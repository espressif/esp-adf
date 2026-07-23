/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_capture_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define AUDIO_RECORD_SAMPLE_RATE       16000
#define AUDIO_RECORD_BITS_PER_SAMPLE   16
#define AUDIO_RECORD_CHANNELS          1
#define AUDIO_RECORD_AAC_BITRATE       64000
#define AUDIO_RECORD_DEFAULT_DURATION  10000
#define AUDIO_RECORD_FRAME_TIMEOUT_MS  100

#define AUDIO_RECORD_STORAGE_DIR        "/sdcard/audio_record"
#define AUDIO_RECORD_MANUAL_MP4         AUDIO_RECORD_STORAGE_DIR "/audio_record.mp4"
#define AUDIO_RECORD_MANUAL_WAV         AUDIO_RECORD_STORAGE_DIR "/ai_audio_record.wav"
#define AUDIO_RECORD_MANUAL_G711_WAV    AUDIO_RECORD_STORAGE_DIR "/audio_record_g711.wav"
#define AUDIO_RECORD_MANUAL_G711_WAV_1  AUDIO_RECORD_STORAGE_DIR "/audio_record_1_g711.wav"

#define AUDIO_RECORD_STREAM0_CODEC  ESP_CAPTURE_FMT_ID_AAC
#define AUDIO_RECORD_STREAM1_CODEC  ESP_CAPTURE_FMT_ID_G711A
#define AUDIO_RECORD_AI_CODEC       ESP_CAPTURE_FMT_ID_PCM

#define DEFAULT_MIC_GAIN                32.0f
#define DEFAULT_VOL                     80
#define AUDIO_RECORD_PLAYER_VOLUME      DEFAULT_VOL
#define AUDIO_RECORD_PLAYBACK_CHANNELS  2
#define AUDIO_RECORD_PLAYER_STACK_SIZE  (8 * 1024)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
