/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __AUDIO_MIXER_H__
#define __AUDIO_MIXER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define ESP_ERR_MIXER_OK      ESP_OK
#define ESP_ERR_MIXER_FAIL    ESP_FAIL
#define ESP_ERR_MIXER_NO_DATA (-2)
#define ESP_ERR_MIXER_TIMEOUT (-3)

#define AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(origin_gain, goal_gain, transfer_time) { \
    .origin_gain_db   = origin_gain,                                                   \
    .goal_gain_db     = goal_gain,                                                     \
    .transfer_time_ms = transfer_time,                                                 \
    .read_cb          = NULL,                                                          \
    .cb_ctx           = NULL,                                                          \
}

#define AUDIO_MIXER_DEFAULT_CONF(samplerate, channels, bits_per_sample, max_slot) { \
    .sample_rate = samplerate,                                                      \
    .channel     = channels,                                                        \
    .bits        = bits_per_sample,                                                 \
    .task_stack  = 4096,                                                            \
    .task_prio   = 6,                                                               \
    .task_core   = 1,                                                               \
    .max_in_slot = max_slot,                                                        \
}

/**
 * @brief  Mixer I/O callback function
 */
typedef esp_err_t (*mixer_io_callback_t)(uint8_t *data, int len, void *ctx);

/**
 * @brief  Enumeration of audio mixer slot identifiers
 */
typedef enum {
    AUDIO_MIXER_SLOT_0 = 0,
    AUDIO_MIXER_SLOT_1,
    AUDIO_MIXER_SLOT_2,
    AUDIO_MIXER_SLOT_3,
    AUDIO_MIXER_SLOT_4,
    AUDIO_MIXER_SLOT_5,
    AUDIO_MIXER_SLOT_6,
    AUDIO_MIXER_SLOT_7,
    AUDIO_MIXER_SLOT_MAX = 8,
} audio_mixer_slot_t;

/**
 * @brief  Enumeration of audio mixer states
 */
typedef enum {
    AUDIO_MIXER_STATE_UNKNOWN,  /*!< Unknown state of the audio mixer */
    AUDIO_MIXER_STATE_IDLE,     /*!< Idle state of the audio mixer */
    AUDIO_MIXER_STATE_RUNNING,  /*!< Running state of the audio mixer */
    AUDIO_MIXER_STATE_ERROR,    /*!< Error state of the audio mixer */
} audio_mixer_state_t;

/**
 * @brief  Configuration structure for the audio mixer
 */
typedef struct {
    uint8_t channel;         /*!< Channel number of the slot */
    uint8_t bits;            /*!< Number of bits in the audio data. Currently just support 16 bits */
    int     sample_rate;     /*!< Sample rate for the audio mixer */
    int     task_stack;      /*!< Task stack size for the audio mixer */
    int     task_prio;       /*!< Task priority for the audio mixer */
    int     task_core;       /*!< Task core for the audio mixer */
    int     max_in_slot;     /*!< Maximum number of input slots for the audio mixer */
    int     max_sample_num;  /*!< Maximum number of audio samples for the audio mixer */
} audio_mixer_config_t;

/**
 * @brief  Structure representing information for an audio mixer slot
 */
typedef struct {
    int8_t              origin_gain_db;    /*!< Original gain of the slot in decibels */
    int8_t              goal_gain_db;      /*!< Goal gain of the slot in decibels */
    int                 transfer_time_ms;  /*!< Transfer time for the slot in milliseconds */
    mixer_io_callback_t read_cb;           /*!< Read callback function for the slot */
    void               *cb_ctx;            /*!< Context to be passed to the read callback function */
} audio_mixer_slot_info_t;

/**
 * @brief  Initializes the audio mixer
 *
 * @param[in]   config  Pointer to the configuration structure for the audio mixer
 * @param[out]  handle  Pointer to the variable that will store the initialized audio mixer handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Init mixer failed because out of memory
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_init(audio_mixer_config_t *config, void **handle);

/**
 * @brief  Deinitializes the audio mixer
 *
 * @param[in]  handle  Handle of the audio mixer to be deinitialized
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_deinit(void *handle);

/**
 * @brief  Configures input slot for the audio mixer
 *
 * @param[in]  handle       Handle of the audio mixer
 * @param[in]  input_slot   Array of input slot information
 * @param[in]  num_of_slot  Number of input slot
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_configure_in(void *handle, audio_mixer_slot_info_t *input_slot, int num_of_slot);

/**
 * @brief  Configures the output callback for the audio mixer
 *
 * @param[in]  handle    Handle of the audio mixer
 * @param[in]  write_cb  Output callback function
 * @param[in]  ctx       Context to be passed to the output callback function
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t audio_mixer_configure_out(void *handle, mixer_io_callback_t write_cb, void *ctx);

/**
 * @brief  Sets the read callback for a specific slot in the audio mixer
 * 
 * @note  This function sets the read callback for a specific slot in the audio mixer
 *
 * @param[in]  handle  Handle of the audio mixer
 * @param[in]  slot    ID of the slot for which the read callback is set
 * @param[in]  read    Read callback function. If you want to cancel the specific slot, can set `read` to NULL
 * @param[in]  ctx     Context to be passed to the read callback function
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_set_read_cb(void *handle, audio_mixer_slot_t slot, mixer_io_callback_t read, void *ctx);

/**
 * @brief  Checks if data is ready for processing in the audio mixer
 *
 * @param[in]  handle  Handle of the audio mixer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_data_is_ready(void *handle);

/**
 * @brief  Gets the current state of the audio mixer
 *
 * @param[in]  handle  Handle of the audio mixer
 * @param[in]  state   Pointer to the variable that will store the retrieved state
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_get_state(void *handle, audio_mixer_state_t *state);

/**
 * @brief  Reset the slot in the audio mixer for the specified slot
 *
 * @note  Function resets the slot for the specified slot in the audio mixer to its initial state.
 *        When you terminate the execution of a slot, you need to continue next time, you need to call this function.
 *
 * @param[in]  handle  Handle to the audio mixer
 * @param[in]  slot    Slot number of the slot to be reset
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_reset_slot(void *handle, audio_mixer_slot_t slot);

/**
 * @brief  Sets the number of channel for a specific slot in the audio mixer
 *
 * @param[in]  handle       Handle of the audio mixer
 * @param[in]  slot         Index of the slot for which the number of slot is set
 * @param[in]  channel_num  Number of channel to be set
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_set_channel(void *handle, audio_mixer_slot_t slot, int channel_num);

/**
 * @brief  Sets the gain for a specific slot in the audio mixer
 *
 * @param[in]  handle  Handle of the audio mixer
 * @param[in]  slot    Index of the slot for which the gain is set
 * @param[in]  gain0   Gain value for the source gain
 * @param[in]  gain1   Gain value for the destination gain
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_set_gain(void *handle, audio_mixer_slot_t slot, float gain0, float gain1);

/**
 * @brief  Sets the transit time for a specific channel in the audio mixer
 *
 * @param[in]  handle           Handle of the audio mixer
 * @param[in]  slot             Index of the slot for which the transit time is set
 * @param[in]  transit_time_ms  Transit time value in milliseconds
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t audio_mixer_set_transit_time(void *handle, audio_mixer_slot_t slot, int transit_time_ms);

#ifdef __cplusplus
}
#endif

#endif
