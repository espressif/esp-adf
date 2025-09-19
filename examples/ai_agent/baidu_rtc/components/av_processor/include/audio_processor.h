/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Brief:  Audio manager configuration */
typedef struct {
    void  *play_dev;
    void  *rec_dev;
    char   mic_layout[8];
    int    board_sample_rate;
    int    board_sample_bits;
    int    board_channels;
    int    play_volume;
    int    rec_volume;
    int    rec_ref_volume;
} audio_manager_config_t;

typedef enum {
    AUDIO_MIXER_FEEDER_BOOST   = 0,
    AUDIO_MIXER_PLAYBACK_BOOST = 1,
    AUDIO_MIXER_BALANCED       = 2,
} audio_mixer_priority_t;

enum audio_player_state_e {
    AUDIO_RUN_STATE_IDLE,
    AUDIO_RUN_STATE_PLAYING,
    AUDIO_RUN_STATE_CLOSED,
};

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
 * @param[in]  config  Pointer to the audio manager configuration structure
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_init(audio_manager_config_t *config);

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
 * @brief  Opens the audio feeder system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_feeder_open(void);

/**
 * @brief  Closes the audio feeder module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_feeder_close(void);

/**
 * @brief  Starts the audio feeder operation.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_feeder_run(void);

/**
 * @brief  Stops the ongoing audio feeder
 *
 * @return
 *       - ESP_OK  On successfully stopping the audio playback
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_feeder_stop(void);

/**
 * @brief  Feeds audio data into the feeder system
 *
 * @param[in]  data       Pointer to the audio data to be fed into the feeder system
 * @param[in]  data_size  Size of the audio data to be fed into the feeder system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_feeder_feed_data(uint8_t *data, int data_size);

/**
 * @brief  Open the audio playback module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_open(void);

/**
 * @brief  Close the audio playback module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_close(void);

/**
 * @brief  Play an audio playback from the specified URL. Eg: "http://192.168.1.100:8000/audio.mp3", "file:///sdcard/audio.mp3"
 *
 * @param[in]  url  URL pointing to the audio playback file to be played
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_play(const char *url);

/**
 * @brief  Get the state of the audio playback module
 *
 * @return
 *       - AUDIO_PLAYBACK_STATE_IDLE  On success
 *       - Other                      Appropriate esp_err_t error code on failure
 */
enum audio_player_state_e audio_playback_get_state(void);

/**
 * @brief  Stop the currently playing audio playback module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_stop(void);

/**
 * @brief  Open the audio mixer module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_mixer_open(void);

/**
 * @brief  Close the audio mixer
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_mixer_close(void);

/**
 * @brief  Switch audio priority between feeder and playback module
 *
 * @param[in]  adjust  The volume adjustment type to switch between foreground/background audio
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_mixer_switch_priority(audio_mixer_priority_t adjust);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
