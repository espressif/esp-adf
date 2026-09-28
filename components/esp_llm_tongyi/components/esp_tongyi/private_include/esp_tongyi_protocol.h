/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_tongyi.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Inputs for the Start (run-task) JSON builder
 */
typedef struct {
    const char  *task_id;                  /*!< Task identifier */
    const char  *workspace_id;             /*!< Workspace identifier */
    const char  *app_id;                   /*!< Application identifier */
    const char  *biz_params_json;          /*!< Optional extra JSON object */
    const char  *up_type;                  /*!< Upstream type */
    const char  *up_mode;                  /*!< Interaction mode */
    const char  *up_audio_format;          /*!< Uplink codec */
    int          up_sample_rate;           /*!< Uplink sample rate in Hz */
    const char  *down_voice;               /*!< TTS voice name */
    const char  *down_audio_format;        /*!< Downlink codec */
    int          down_sample_rate;         /*!< Downlink sample rate in Hz */
    int          down_frame_size;          /*!< Frame size in milliseconds */
    int          down_volume;              /*!< Output volume */
    int          down_bit_rate;            /*!< Bitrate in kbps */
    int          down_transmit_rate_limit; /*!< Transmit rate limit in bytes */
    const char  *user_id;                  /*!< User identifier; empty falls back to task_id */
    const char  *device_uuid;              /*!< Optional device UUID */
} esp_tongyi_proto_start_cfg_t;

/**
 * @brief  Callbacks invoked while parsing a server JSON message
 */
typedef struct {
    void  *ctx;                                                        /*!< User context */
    void (*on_started)(void *ctx, const char *dialog_id);              /*!< Started with dialog_id */
    void (*on_dialog_state_changed)(void *ctx, const char *state_str); /*!< Dialog state string */
    void (*on_speech_started)(void *ctx);                              /*!< Speech started */
    void (*on_responding_started)(void *ctx);                          /*!< Responding started */
    void (*on_responding_ended)(void *ctx);                            /*!< Responding ended */
    void (*on_speech_content)(void *ctx, const char *asr_text);        /*!< ASR text */
    void (*on_responding_content)(void *ctx, const char *raw_message); /*!< Reply JSON */
    void (*on_error)(void *ctx, const char *raw_message);              /*!< Error JSON */
    void (*on_task_failed)(void *ctx, const char *raw_message);        /*!< task-failed JSON */
} esp_tongyi_proto_parse_handler_t;

/**
 * @brief  Map a public directive to the DashScope directive string
 *
 * @param[in]  directive  Public directive
 *
 * @return
 *       - Non-NULL DashScope name  If @p directive is valid
 *       - NULL                     If @p directive is invalid
 */
const char *esp_tongyi_proto_directive_name(esp_tongyi_speech_action_t directive);

/**
 * @brief  Build the Start (run-task) JSON
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  cfg        Start payload fields
 * @param[in]  dialog_id  Optional dialog ID to resume; may be NULL
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_start(const esp_tongyi_proto_start_cfg_t *cfg, const char *dialog_id);

/**
 * @brief  Build the Stop JSON
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  task_id    Task identifier
 * @param[in]  dialog_id  Dialog identifier; may be NULL
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_stop(const char *task_id, const char *dialog_id);

/**
 * @brief  Build a continue-task directive JSON
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  task_id    Task identifier
 * @param[in]  dialog_id  Dialog identifier
 * @param[in]  directive  DashScope directive name
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_directive(const char *task_id, const char *dialog_id, const char *directive);

/**
 * @brief  Build a RequestToRespond text JSON
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  task_id        Task identifier
 * @param[in]  dialog_id      Dialog identifier
 * @param[in]  text           Request text
 * @param[in]  is_transcript  True to use type=transcript
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_text(const char *task_id, const char *dialog_id,
                                  const char *text, bool is_transcript);

/**
 * @brief  Build RequestToRespond that opens LiveAI `voicechat_video_channel`
 *
 *         Caller frees the returned string with free().
 */
char *esp_tongyi_proto_build_video_channel(const char *task_id, const char *dialog_id);

/**
 * @brief  Build a RequestToRespond or UpdateInfo image JSON
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  task_id       Task identifier
 * @param[in]  dialog_id     Dialog identifier
 * @param[in]  base64_image  Base64-encoded image
 * @param[in]  prompt        Required unless @p update_info is true
 * @param[in]  update_info   True to send UpdateInfo
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_image(const char *task_id, const char *dialog_id,
                                   const char *base64_image, const char *prompt,
                                   bool update_info);

/**
 * @brief  Wrap a caller-built payload JSON object in continue-task
 *
 *         Caller frees the returned string with free().
 *
 * @param[in]  task_id       Task identifier
 * @param[in]  payload_json  JSON object string
 *
 * @return
 *       - Heap JSON string  On success
 *       - NULL              If construction fails
 */
char *esp_tongyi_proto_build_payload_wrap(const char *task_id, const char *payload_json);

/**
 * @brief  Parse one server JSON message and invoke matching handlers
 *
 * @param[in]  message  NUL-terminated JSON message
 * @param[in]  handler  Callback table; must not be NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p message or @p handler is NULL
 *       - ESP_FAIL             If JSON parsing fails
 */
esp_err_t esp_tongyi_proto_parse_message(const char *message, esp_tongyi_proto_parse_handler_t *handler);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
