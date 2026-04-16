/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_audio_types.h"
#include "esp_gmf_data_bus.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio event types
 */
typedef enum {
    APP_AUDIO_EVENT_RECORD_START,   /*!< Record start */
    APP_AUDIO_EVENT_RECORD_STOP,    /*!< Record stop */
    APP_AUDIO_EVENT_PLAY_START,     /*!< Playback start */
    APP_AUDIO_EVENT_PLAY_STOP,      /*!< Playback stop */
    APP_AUDIO_EVENT_PLAY_FINISHED,  /*!< Playback finished */
    APP_AUDIO_EVENT_ERROR,          /*!< Error event */
} app_audio_event_t;

/**
 * @brief  Audio event callback function type
 *
 * @param[in]  event     Event type
 * @param[in]  user_ctx  User context
 */
typedef void (*app_audio_event_cb_t)(app_audio_event_t event, void *user_ctx);

/**
 * @brief  Audio format configuration
 */
typedef struct {
    uint32_t  sample_rate;      /*!< Sample rate (8000-48000) */
    uint8_t   channels;         /*!< Channel count (1/2/4) */
    uint8_t   bits_per_sample;  /*!< Bits per sample (16/24/32) */
} app_audio_format_t;

/**
 * @brief  Record configuration
 */
typedef struct {
    app_audio_format_t    format;          /*!< Audio format */
    bool                  enable_afe;      /*!< Enable AFE (echo cancellation/noise reduction) */
    bool                  enable_vad;      /*!< Enable VAD (voice activity detection) */
    const char           *file_path;       /*!< Record file path (NULL for ringbuffer mode) */
    uint32_t              buffer_size;     /*!< Ringbuffer size (valid when file_path is NULL) */
    app_audio_event_cb_t  event_callback;  /*!< Event callback function */
    void                 *user_ctx;        /*!< User context */
    uint32_t              encode_type;     /*!< Encode type (0=PCM/ESP_AUDIO_TYPE_PCM, others refer to esp_audio_types.h) */
} app_audio_record_config_t;

/**
 * @brief  Playback configuration
 */
typedef struct {
    app_audio_format_t    format;          /*!< Audio format (source file format) */
    const char           *file_path;       /*!< Playback file path (NULL for ringbuffer mode) */
    uint32_t              buffer_size;     /*!< Ringbuffer size (valid when file_path is NULL) */
    app_audio_event_cb_t  event_callback;  /*!< Event callback function */
    void                 *user_ctx;        /*!< User context */
    bool                  loop;            /*!< Loop playback */
    uint32_t              decode_type;     /*!< Decode type (0=PCM/ESP_AUDIO_TYPE_PCM, others refer to esp_audio_types.h) */
} app_audio_playback_config_t;

/**
 * @brief  Audio system initialization configuration
 */
typedef struct {
    app_audio_format_t  board_format;  /*!< Board codec format */
} app_audio_init_config_t;

/**
 * @brief  Default initialization configuration
 */
#define APP_AUDIO_INIT_CONFIG_DEFAULT()  {  \
    .board_format = {                       \
        .sample_rate     = 16000,           \
        .channels        = 2,               \
        .bits_per_sample = 16,              \
    },                                      \
}

/**
 * @brief  Default record configuration
 */
#define APP_AUDIO_RECORD_CONFIG_DEFAULT()  {  \
    .format = {                               \
        .sample_rate     = 16000,             \
        .channels        = 2,                 \
        .bits_per_sample = 16,                \
    },                                        \
    .enable_afe     = false,                  \
    .enable_vad     = false,                  \
    .file_path      = NULL,                   \
    .buffer_size    = 0,                      \
    .event_callback = NULL,                   \
    .user_ctx       = NULL,                   \
    .encode_type    = 0,                      \
}

/**
 * @brief  Default playback configuration
 */
#define APP_AUDIO_PLAYBACK_CONFIG_DEFAULT()  {  \
    .format = {                                 \
        .sample_rate     = 16000,               \
        .channels        = 1,                   \
        .bits_per_sample = 16,                  \
    },                                          \
    .file_path      = NULL,                     \
    .buffer_size    = 0,                        \
    .event_callback = NULL,                     \
    .user_ctx       = NULL,                     \
    .loop           = false,                    \
    .decode_type    = 0,                        \
}

/**
 * @brief  Initialize audio system
 *
 * @param[in]  config  Initialization configuration, NULL for default
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_init(app_audio_init_config_t *config);

/**
 * @brief  Deinitialize audio system
 */
void app_audio_deinit(void);

/**
 * @brief  Start recording
 *
 * @param[in]  config  Record configuration
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_record_start(app_audio_record_config_t *config);

/**
 * @brief  Stop recording
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_record_stop(void);

/**
 * @brief  Start playback
 *
 * @param[in]  config  Playback configuration
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_playback_start(app_audio_playback_config_t *config);

/**
 * @brief  Stop playback
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_playback_stop(void);

/**
 * @brief  Check if recording
 *
 * @return
 *       - true   If recording
 *       - false  If not recording
 */
bool app_audio_is_recording(void);

/**
 * @brief  Check if playing
 *
 * @return
 *       - true   If playing
 *       - false  If not playing
 */
bool app_audio_is_playing(void);

/**
 * @brief  Set playback volume
 *
 * @param[in]  volume  Volume (0-100)
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_set_volume(int volume);

/**
 * @brief  Set microphone gain
 *
 * @param[in]  gain  Gain in dB (-120 to 40)
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_set_mic_gain(int gain);

/**
 * @brief  Set microphone channel gain
 *
 * @param[in]  channel_mask  Channel mask (e.g. 0x01=CH0, 0x02=CH1, 0x03=CH0+CH1, 0x04=CH2)
 * @param[in]  gain          Gain (0-100)
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Failed
 */
esp_err_t app_audio_set_mic_channel_gain(uint32_t channel_mask, int gain);

/**
 * @brief  Get record ringbuffer handle
 *
 * @return
 *       - Handle  Ringbuffer handle if recording
 *       - NULL    If not recording
 */
esp_gmf_db_handle_t app_audio_get_record_buffer(void);

/**
 * @brief  Get playback ringbuffer handle
 *
 * @return
 *       - Handle  Ringbuffer handle if playing
 *       - NULL    If not playing
 */
esp_gmf_db_handle_t app_audio_get_playback_buffer(void);

/**
 * @brief  Get playback ringbuffer free space percentage
 *
 * @param[out]  free_percent  Pointer to store the free space percentage (0-100)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If playback is not running or ringbuffer not available
 *       - ESP_FAIL               Failed to get buffer information
 */
esp_err_t app_audio_get_playback_buffer_free_percent(uint8_t *free_percent);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
