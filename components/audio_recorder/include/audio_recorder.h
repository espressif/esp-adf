/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _AUDIO_RECORDER_H_
#define _AUDIO_RECORDER_H_

#include "recorder_encoder_iface.h"
#include "recorder_sr_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_REC_DEF_TASK_SZ   (4 * 1024) /*!< Stack size of recorder task */
#define AUDIO_REC_DEF_TASK_PRIO (10)       /*!< Priority of recoder task */
#define AUDIO_REC_DEF_TASK_CORE (1)        /*!< Pinned to core */

#define AUDIO_REC_DEF_WAKEUP_TM       (10000) /*!< Default wake up time (ms) */
#define AUDIO_REC_DEF_WAKEEND_TM      (900)   /*!< Duration after vad off (ms) */
#define AUDIO_REC_VAD_START_SPEECH_MS (160)   /*!< Consecutive speech frame will be judged to vad start (ms) */
#define AUDIO_REC_DEF_VAD_OFF_TM      (300)   /*!< Default vad off time (ms) */

/**
 * @brief Recorder event
 */
typedef enum {
    AUDIO_REC_WAKEUP_START = -100, /*!< Wakeup start */
    AUDIO_REC_WAKEUP_END,          /*!< Wakeup stop */
    AUDIO_REC_VAD_START,           /*!< Vad start */
    AUDIO_REC_VAD_END,             /*!< Vad stop */
    AUDIO_REC_COMMAND_DECT = 0     /*!< Form 0 is the id of the voice commands detected by Multinet*/
    /* DO NOT add items below this line */
} audio_rec_evt_t;

/**
 * @brief Event Notification
 */
typedef esp_err_t (*rec_event_cb_t)(audio_rec_evt_t event, void *user_data);

/**
 * @brief Audio recorder configuration
 */
typedef struct {
    int                       pinned_core;     /*!< Audio recorder task pinned to core */
    int                       task_prio;       /*!< Audio recorder task priority */
    int                       task_size;       /*!< Audio recorder task stack size */
    rec_event_cb_t            event_cb;        /*!< Event callback function, event type as audio_rec_evt_t shown above*/
    void                      *user_data;      /*!< Pointer to user data (optional) */
    recorder_data_read_t      read;            /*!< Data callback function used feed data to audio recorder */
    void                      *sr_handle;      /*!< SR handle */
    recorder_sr_iface_t       *sr_iface;       /*!< SR interface */
    int                       wakeup_time;     /*!< Unit:ms. The duration that the wakeup state remains when VAD is not triggered */
    int                       vad_start;       /*!< Unit:ms. Consecutive speech frame will be judged to vad start*/
    int                       vad_off;         /*!< Unit:ms. When the silence time exceeds this value, it is determined as AUDIO_REC_VAD_END state */
    int                       wakeup_end;      /*!< Unit:ms. When the silence time after AUDIO_REC_VAD_END state exceeds this value, it is determined as AUDIO_REC_WAKEUP_END */
    void                      *encoder_handle; /*!< Encoder handle */
    recorder_encoder_iface_t  *encoder_iface;  /*!< Encoder interface */
} audio_rec_cfg_t;

/**
 * @brief Audio recorder handle
 */
typedef struct __audio_recorder *audio_rec_handle_t;

#define AUDIO_RECORDER_DEFAULT_CFG()                     \
    {                                                    \
        .pinned_core    = AUDIO_REC_DEF_TASK_CORE,       \
        .task_prio      = AUDIO_REC_DEF_TASK_PRIO,       \
        .task_size      = AUDIO_REC_DEF_TASK_SZ,         \
        .event_cb       = NULL,                          \
        .user_data      = NULL,                          \
        .read           = NULL,                          \
        .sr_handle      = NULL,                          \
        .sr_iface       = NULL,                          \
        .wakeup_time    = AUDIO_REC_DEF_WAKEUP_TM,       \
        .vad_start      = AUDIO_REC_VAD_START_SPEECH_MS, \
        .vad_off        = AUDIO_REC_DEF_VAD_OFF_TM,      \
        .wakeup_end     = AUDIO_REC_DEF_WAKEEND_TM,      \
        .encoder_handle = NULL,                          \
        .encoder_iface  = NULL,                          \
    }

/**
 * @brief Initialize and start up audio recorder
 *
 * @param cfg Configuration of audio recorder
 *
 * @return NULL    failed
 *         Others  audio recorder handle
 */
audio_rec_handle_t audio_recorder_create(audio_rec_cfg_t *cfg);

/**
 * @brief Start recording by force
 *
 * @note If there need to read from recorder without wake word detected
 *       or read from recorder while the wake word detection is disabled,
 *       this interface can be use to force start the recorder process.
 *
 * @param handle Audio recorder handle
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_trigger_start(audio_rec_handle_t handle);

/**
 * @brief Stop recording by force
 *
 * @note No matter the recorder process is triggered by wake word detected or triggered by `audio_recorder_trigger_start`,
 *       this function can be used to force stop the recorder.
 *       And if the VAD detection is disabeled, this must be invoked to stop recording after `audio_recorder_trigger_start`.
 *
 * @param handle Audio recorder handle
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_trigger_stop(audio_rec_handle_t handle);

/**
 * @brief Enable or suspend wake word detection
 *
 * @param handle Audio recorder handle
 * @param enable true: enable wake word detection
 *               false: disable wake word detection
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_wakenet_enable(audio_rec_handle_t handle, bool enable);

/**
 * @brief Enable or suspend speech command recognition
 *
 * @param handle Audio recorder handle
 * @param enable true: enable speech command recognition
 *               false: disable speech command recognition
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_multinet_enable(audio_rec_handle_t handle, bool enable);

/**
 * @brief Enable or suspend voice duration check
 *
 * @param handle Audio recorder handle
 * @param enable true: enable voice duration check
 *               false: disable voice duration check
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_vad_check_enable(audio_rec_handle_t handle, bool enable);

/**
 * @brief Read data from audio recorder
 *
 * @param handle  Audio recorder handle
 * @param buffer  Buffer to save data
 * @param length  Size of buffer
 * @param ticks   Timeout for reading
 *
 * @return Length of data actually read
 *         ESP_ERR_INVALID_ARG
 */
int audio_recorder_data_read(audio_rec_handle_t handle, void *buffer, int length, TickType_t ticks);

/**
 * @brief Destroy audio recorder and recycle all resource
 *
 * @param handle Audio recorder handle
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t audio_recorder_destroy(audio_rec_handle_t handle);

/**
 * @brief Get the wake up state of audio recorder
 *
 * @param handle Audio recorder handle
 *
 * @return true
 *         false
 */
bool audio_recorder_get_wakeup_state(audio_rec_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
