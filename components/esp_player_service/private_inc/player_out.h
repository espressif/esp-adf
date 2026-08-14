/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"

#include "esp_err.h"

#include "esp_player_service_setup.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Whether the audio output domain is built
 *
 * @note  Follows CONFIG_ESP_PLAYER_ENABLE_AUDIO, the same switch that gates the
 *        engine's audio decode / render path, so the two can never disagree: a
 *        mixer the engine refuses to feed is not a reachable configuration.
 *        Lets the core ask about the capability without naming the option.
 *
 * @note  When 0, the calls below resolve to the stub implementation: creating a
 *        binding with no sink still succeeds so a silent video build can be
 *        configured, everything else reports ESP_ERR_NOT_SUPPORTED.
 */
#ifdef CONFIG_ESP_PLAYER_ENABLE_AUDIO
#define PLAYER_OUT_AUDIO_SUPPORTED  1
#else
#define PLAYER_OUT_AUDIO_SUPPORTED  0
#endif  /* CONFIG_ESP_PLAYER_ENABLE_AUDIO */

/**
 * @brief  Audio output domain handle
 */
typedef struct player_out *player_out_handle_t;

/**
 * @brief  Mixer task parameters
 *
 * @note  Defaults come from esp_audio_render Kconfig, see
 *        player_out_audio_default_task_cfg(). The core overlays the service
 *        scheduler on top before player_out_audio_set_task_cfg().
 */
typedef struct {
    uint32_t  stack;         /*!< Task stack size in bytes */
    uint8_t   prio;          /*!< Task priority */
    int8_t    core;          /*!< Core id; negative for no affinity */
    bool      stack_in_ext;  /*!< True to place the stack in external RAM */
} player_out_task_cfg_t;

/**
 * @brief  Per-slot mixer gain
 */
typedef struct {
    float     initial_gain;   /*!< Gain applied while the slot is ducked */
    float     target_gain;    /*!< Gain applied while the slot is in foreground */
    uint32_t  transition_ms;  /*!< Transition time between the two gains */
} player_out_mixer_gain_t;

/**
 * @brief  Audio output binding
 *
 * @note  Authored by whoever opens the PCM sink, so `writer` / `codec_dev` and
 *        `out_fmt` always come from the same place. `writer` wins over
 *        `codec_dev`; with neither set the binding exists but stays closed.
 */
typedef struct {
    esp_player_service_write_cb_t  writer;         /*!< Custom PCM writer; NULL uses codec_dev */
    void                          *writer_ctx;     /*!< Context passed to writer */
    void                          *codec_dev;      /*!< Codec device; used when writer is NULL */
    esp_player_service_pcm_fmt_t   out_fmt;        /*!< Device / mixer output format; every field required */
    uint8_t                        max_slots;      /*!< Mixer input count (= service stream slots) */
    uint8_t                        device_volume;  /*!< Initial device volume 0-100 */
    void                          *pool;           /*!< GMF pool used by the mixer and slot processors */
} player_out_audio_cfg_t;

/**
 * @brief  Register the GMF elements the audio render pipeline needs
 *
 * @note  Module level, not per binding: the core builds its default pool during
 *        create(), long before a binding exists. The pool itself stays with the
 *        core because the video subclass registers into the same one. The stub
 *        build registers nothing and reports success, leaving an audio-free pool.
 *
 * @param[in]  pool  GMF pool handle as void *
 *
 * @return
 *       - ESP_OK               On success, or when audio output is not built
 *       - ESP_ERR_INVALID_ARG  pool is NULL
 *       - ESP_ERR_NO_MEM       An element failed to init or register
 */
esp_err_t player_out_audio_register_elements(void *pool);

/**
 * @brief  Read the built-in mixer task defaults
 *
 * @param[out]  out_cfg  Receives the defaults; must not be NULL
 */
void player_out_audio_default_task_cfg(player_out_task_cfg_t *out_cfg);

/**
 * @brief  Create an audio output binding
 *
 * @note  Creates no render and opens no device. Call player_out_audio_open()
 *        once a sink is available.
 *
 * @param[in]   cfg         Output binding; copied
 * @param[out]  out_handle  Receives the binding handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_handle is NULL, max_slots is 0, or a
 *                              field of out_fmt is 0
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t player_out_audio_create(const player_out_audio_cfg_t *cfg, player_out_handle_t *out_handle);

/**
 * @brief  Close and release an audio output binding
 *
 * @note  Clears *handle so the caller cannot keep a dangling binding.
 *
 * @param[in,out]  handle  Address of the binding handle; NULL or *NULL is a no-op
 */
void player_out_audio_destroy(player_out_handle_t *handle);

/**
 * @brief  Override the mixer task parameters
 *
 * @note  Applied on the next player_out_audio_open().
 *
 * @param[in]  handle  Binding handle
 * @param[in]  cfg     Task parameters
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle or cfg is NULL
 */
esp_err_t player_out_audio_set_task_cfg(player_out_handle_t handle, const player_out_task_cfg_t *cfg);

/**
 * @brief  Create the mixer and open the PCM sink
 *
 * @note  Idempotent while open.
 *
 * @param[in]  handle  Binding handle
 *
 * @return
 *       - ESP_OK                 On success or already open
 *       - ESP_ERR_INVALID_ARG    handle is NULL
 *       - ESP_ERR_INVALID_STATE  No writer and no codec device
 *       - Others                 Mixer creation or device open failed
 */
esp_err_t player_out_audio_open(player_out_handle_t handle);

/**
 * @brief  Destroy the mixer and close the PCM sink
 *
 * @note  Drops every slot handle. The binding itself stays valid, so a later
 *        player_out_audio_open() rebuilds the same output.
 *
 * @param[in]  handle  Binding handle; NULL is a no-op
 */
void player_out_audio_close(player_out_handle_t handle);

/**
 * @brief  Check whether the mixer exists
 *
 * @param[in]  handle  Binding handle
 *
 * @return
 *       - true   Mixer created
 *       - false  handle is NULL or output is closed
 */
bool player_out_audio_is_open(player_out_handle_t handle);

/**
 * @brief  Check whether a PCM sink was bound
 *
 * @param[in]  handle  Binding handle
 *
 * @return
 *       - true   A writer or a codec device is available
 *       - false  handle is NULL or the audio path is deferred
 */
bool player_out_audio_has_sink(player_out_handle_t handle);

/**
 * @brief  Fetch one mixer slot and attach its processors
 *
 * @note  The returned handle goes to esp_player_config_t::audio_render_hd.
 *        esp_player opens and closes the stream; this module never does.
 *        A pool without ALC / sonic is not an error: the slot works, volume
 *        control stays inactive.
 *
 * @param[in]   handle    Binding handle
 * @param[in]   idx       Slot index, less than max_slots
 * @param[out]  out_slot  Receives the opaque slot handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    handle or out_slot is NULL, or idx is out of range
 *       - ESP_ERR_INVALID_STATE  Output is not open
 *       - Others                 Mixer rejected the slot
 */
esp_err_t player_out_slot_acquire(player_out_handle_t handle, uint8_t idx, void **out_slot);

/**
 * @brief  Forget one mixer slot
 *
 * @param[in]  handle  Binding handle; NULL is a no-op
 * @param[in]  idx     Slot index
 */
void player_out_slot_release(player_out_handle_t handle, uint8_t idx);

/**
 * @brief  Program the slot ALC gain from a UI volume
 *
 * @param[in]  handle    Binding handle
 * @param[in]  idx       Slot index
 * @param[in]  volume    UI volume 0-100
 * @param[in]  channels  Slot input channel count; 0 uses 2
 *
 * @return
 *       - ESP_OK               On success, or the slot has no ALC yet
 *       - ESP_ERR_INVALID_ARG  handle is NULL or idx is out of range
 *       - ESP_ERR_NOT_FOUND    ALC element missing from the slot
 *       - ESP_FAIL             ALC rejected the gain
 */
esp_err_t player_out_slot_set_volume(player_out_handle_t handle, uint8_t idx,
                                     uint8_t volume, uint8_t channels);

/**
 * @brief  Program the slot mixer gain
 *
 * @note  The mixer only accepts gain changes while no slot is running, so the
 *        core retries this until it succeeds.
 *
 * @param[in]  handle  Binding handle
 * @param[in]  idx     Slot index
 * @param[in]  gain    Gain values
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    handle or gain is NULL, or idx is out of range
 *       - ESP_ERR_INVALID_STATE  Output is not open
 *       - Others                 Mixer rejected the gain
 */
esp_err_t player_out_slot_set_gain(player_out_handle_t handle, uint8_t idx,
                                   const player_out_mixer_gain_t *gain);

/**
 * @brief  Fade one slot towards its target or initial mixer gain
 *
 * @param[in]  handle   Binding handle
 * @param[in]  idx      Slot index
 * @param[in]  fade_in  True to fade towards target gain
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    handle is NULL or idx is out of range
 *       - ESP_ERR_INVALID_STATE  Slot was never acquired
 *       - Others                 Mixer rejected the fade
 */
esp_err_t player_out_slot_set_fade(player_out_handle_t handle, uint8_t idx, bool fade_in);

/**
 * @brief  Apply the device output volume
 *
 * @note  Stored for the next device open even when no device is bound yet.
 *
 * @param[in]  handle  Binding handle
 * @param[in]  volume  Device volume 0-100
 *
 * @return
 *       - ESP_OK               On success, or no device bound
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 *       - ESP_FAIL             Device rejected the volume
 */
esp_err_t player_out_set_device_volume(player_out_handle_t handle, uint8_t volume);

/**
 * @brief  Get the mixer handle for advanced use
 *
 * @param[in]  handle  Binding handle
 *
 * @return
 *       - Non-NULL  esp_audio_render_handle_t as void *
 *       - NULL      handle is NULL or output is closed
 */
void *player_out_get_render(player_out_handle_t handle);

/**
 * @brief  Get the bound codec device
 *
 * @param[in]  handle  Binding handle
 *
 * @return
 *       - Non-NULL  Codec device handle
 *       - NULL      handle is NULL or a custom writer is in use
 */
void *player_out_get_codec_dev(player_out_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
