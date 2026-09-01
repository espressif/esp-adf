/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_PLAYER_SERVICE_DEFAULT_SAMPLE_RATE      48000
#define ESP_PLAYER_SERVICE_DEFAULT_BITS_PER_SAMPLE  16
#define ESP_PLAYER_SERVICE_DEFAULT_CHANNEL          2
#define ESP_PLAYER_SERVICE_DEFAULT_OUTPUT_VOLUME    60

/**
 * @brief  PCM writer callback for a custom audio output
 *
 *         Called from the mixer / render task with interleaved PCM in the
 *         format declared by esp_player_service_pcm_fmt_t. Must not block
 *         longer than one process period.
 *
 * @param[in]  pcm_data  PCM data to consume
 * @param[in]  pcm_size  PCM data size in bytes
 * @param[in]  ctx       Context from esp_player_service_setup_t::out_ctx
 *
 * @return
 *       - 0       On success
 *       - Others  Failed to write
 */
typedef int (*esp_player_service_write_cb_t)(uint8_t *pcm_data, uint32_t pcm_size, void *ctx);

/**
 * @brief  Fixed PCM format of the audio output
 *
 *         Must match the format the output device is actually driven with. A
 *         zero field falls back to ESP_PLAYER_SERVICE_DEFAULT_* (48000 / 16 / 2).
 */
typedef struct {
    uint32_t  sample_rate;      /*!< Sample rate in Hz */
    uint8_t   bits_per_sample;  /*!< Bit depth per sample */
    uint8_t   channel;          /*!< Channel count */
} esp_player_service_pcm_fmt_t;

/**
 * @brief  Player service setup configuration
 *
 *         Holds the audio output binding only: which sink to write to, and the
 *         format that sink is opened with. Audio output selection (first match
 *         wins): `out_writer` → `codec_dev` → neither (defer audio render).
 *         Board DAC lookup belongs in the audio subclass.
 *
 *         Video output is not part of this struct: the video subclass installs its
 *         render separately, so an audio-only apply_setup can never drop the
 *         display. Streams are equal: this session's A/V follows URL / feed / link
 *         plus whether a video output is attached.
 *
 * @note  `fixed_out_sample_info` is a contract, not a preference: the codec device
 *        is opened with it and the mixer renders into it, so whoever declares the
 *        sink must declare its format in the same call.
 */
typedef struct {
    void                          *codec_dev;              /*!< Codec device; ignored if out_writer set */
    esp_player_service_write_cb_t  out_writer;             /*!< Custom PCM writer; NULL uses codec_dev */
    void                          *out_ctx;                /*!< Context passed to out_writer */
    esp_player_service_pcm_fmt_t   fixed_out_sample_info;  /*!< DAC / render format; 0 uses ESP_PLAYER_SERVICE_DEFAULT_* (48000 / 16 / 2) */
} esp_player_service_setup_t;

/**
 * @brief  Default output setup
 *
 *         Prefer this macro then override fields. SETUP_DEFAULT leaves the audio
 *         output NULL (defer) for unit tests; apps typically set `codec_dev` /
 *         `out_writer` here (board DAC is audio-subclass setup).
 */
#define ESP_PLAYER_SERVICE_SETUP_DEFAULT()  {                           \
    .codec_dev             = NULL,                                      \
    .out_writer            = NULL,                                      \
    .out_ctx               = NULL,                                      \
    .fixed_out_sample_info = {                                          \
        .sample_rate     = ESP_PLAYER_SERVICE_DEFAULT_SAMPLE_RATE,      \
        .bits_per_sample = ESP_PLAYER_SERVICE_DEFAULT_BITS_PER_SAMPLE,  \
        .channel         = ESP_PLAYER_SERVICE_DEFAULT_CHANNEL,          \
    },                                                                  \
}

/**
 * @brief  Apply output setup to a player service
 *
 *         Must be called while the service is INITIALIZED (not started). May be
 *         called multiple times to change settings; any existing audio render is
 *         destroyed and recreated. Prefer ESP_PLAYER_SERVICE_SETUP_DEFAULT() then
 *         override fields.
 *
 * @note  Touches the audio output only. A video render installed by the video
 *        subclass survives this call.
 *
 * @note  Any player already built for a stream is destroyed, together with its
 *        URL and feed track declarations: its A/V mask was derived from the sink
 *        this call replaces. Declare the source again after applying a new setup.
 *
 * @note  Once the previous render has been destroyed, a failure leaves the service
 *        unconfigured; the previous output setup is not restored. Apply a valid setup
 *        again before playing.
 *
 * @param[in]  service  Player service handle
 * @param[in]  cfg      Output / codec or custom-writer configuration
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If the service is already started
 *       - ESP_ERR_NO_MEM         If allocation fails
 *       - Others                 If render / codec setup fails
 */
esp_err_t esp_player_service_apply_setup(esp_player_service_t *service,
                                         const esp_player_service_setup_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
