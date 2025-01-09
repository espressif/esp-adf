#pragma once

#include "audio_element.h"
#include "recorder_sr.h"

/**
 * @brief  Create an encoder stream for recording audio
 *
 * @return
 *       - Handle to the audio element representing the encoder stream
 *       - NULL if the creation fails.
 */
audio_element_handle_t create_record_encoder_stream(void);

/**
 * @brief  Create a raw audio stream for recording
 *
 * @return
 *      - Handle to the audio element representing the raw stream
 *      - NULL if the creation fails.
 */
audio_element_handle_t create_record_raw_stream(void);

/**
 * @brief  Create an I2S stream for recording audio
 *
 * @return
 *       - Handle to the audio element representing the I2S stream
 *       - NULL if the creation fails.
 */
audio_element_handle_t create_record_i2s_stream(void);

/**
 * @brief  Create a decoder stream for audio playback
 *
 * @return
 *      - Handle to the audio element representing the decoder stream
 *      - NULL if the creation fails.
 */
audio_element_handle_t create_player_decoder_stream(void);

/**
 * @brief  Create a WAV decoder stream for audio playback
 *
 * @return
 *      - Handle to the audio element representing the WAV decoder stream
*       - NULL if the creation fails.
 */
audio_element_handle_t create_player_wav_decoder_stream(void);

/**
 * @brief  Create a raw audio stream for playback
 *
 * @return
 *       - Handle to the audio element representing the raw stream
 *       - NULL if the creation fails.
 */
audio_element_handle_t create_player_raw_stream(void);

/**
 * @brief  Create an I2S stream for audio playback
 *
 * @return
 *       - Handle to the audio element representing the I2S stream
 *       - NULL if the creation fails.
 */
audio_element_handle_t create_player_i2s_stream(bool enable_task);

/**
 * @brief  Create a stream element to convert mono-channel (1 channel) audio data to stereo-channel (2 channels).
 *
 * @return 
 *     - A handle to the audio element on success.
 *     - NULL if the creation fails.
 */
audio_element_handle_t create_ch1_to_ch2_rsp_stream(void);

/**
 * @brief  Create a stream element to convert mono-channel 8 kHz audio to stereo-channel 16 kHz audio.
 *
 * @return 
 *     - A handle to the audio element on success.
 *     - NULL if the creation fails.
 */
audio_element_handle_t create_8k_ch1_to_16k_ch2_rsp_stream(void);

/**
 * @brief  Create a SPIFFS stream for audio player file input
 *
 * @return
 *      - Handle to the audio element representing the SPIFFS stream
*       - NULL if the creation fails.
 */
audio_element_handle_t create_audio_player_spiffs_stream(void);

/**
 * @brief Get the default audio recording configuration.
 *
 * @return Default audio recording configuration
 */
recorder_sr_cfg_t get_default_audio_record_config(void);

