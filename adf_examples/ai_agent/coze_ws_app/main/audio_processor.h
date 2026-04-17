/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Type definition for the audio recorder event callback function
 *
 * @param[in]  event  Pointer to the event data, which can contain information about the recorder's state
 * @param[in]  ctx    User-defined context pointer, passed when registering the callback
 */
typedef void (*recorder_event_callback_t)(void *event, void *ctx);

/**
 * @brief  Initializes the audio manager module.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_init();

/**
 * @brief  Deinitializes the audio manager component
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_deinit();

/**
 * @brief  Opens the audio recorder and registers an event callback
 *
 * @param[in]  cb   Pointer to the event callback function, which will be invoked on recorder events
 * @param[in]  ctx  User-defined context pointer passed to the callback function
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx);

/**
 * @brief  Closes the audio recorder
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_close(void);

/**
 * @brief  Reads audio data from the recorder.
 *
 * @param[out]  data       Pointer to the buffer where the audio data will be stored
 * @param[in]   data_size  Size of the buffer to store the audio data.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_read_data(uint8_t *data, int data_size);

/**
 * @brief  Opens the audio playback system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_open();

/**
 * @brief  Closes the audio playback module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_close(void);

/**
 * @brief  Starts the audio playback operation.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_run(void);

/**
 * @brief  Stops the ongoing audio playback
 *
 * @return
 *       - ESP_OK  On successfully stopping the audio playback
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_stop(void);

/**
 * @brief  Feeds audio data into the playback system
 *
 * @param[in]  data       Pointer to the audio data to be fed into the playback system
 * @param[in]  data_size  Size of the audio data to be played
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_feed_data(uint8_t *data, int data_size);

/**
 * @brief  Opens the audio prompt system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_open(void);

/**
 * @brief  Closes the audio prompt functionality
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_close(void);

/**
 * @brief  Plays an audio prompt from the specified URL
 *
 * @param[in]  url  URL pointing to the audio prompt file to be played
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_play(const char *url);

/**
 * @brief  Stops the currently playing audio prompt
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_stop(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
