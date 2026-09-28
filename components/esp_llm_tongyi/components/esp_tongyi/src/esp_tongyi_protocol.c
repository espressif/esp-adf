/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "esp_tongyi_protocol.h"

static const char *TAG = "TONGYI_PROTO";

#define TONGYI_JSON_STREAMING_DEFAULT  "duplex"

const char *esp_tongyi_proto_directive_name(esp_tongyi_speech_action_t directive)
{
    switch (directive) {
        case ESP_TONGYI_SPEECH_ACTION_SEND:
            return "SendSpeech";
        case ESP_TONGYI_SPEECH_ACTION_STOP:
            return "StopSpeech";
        case ESP_TONGYI_SPEECH_ACTION_CANCEL:
            return "CancelSpeech";
        case ESP_TONGYI_SPEECH_ACTION_REQUEST_TO_SPEAK:
            return "RequestToSpeak";
        case ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_STARTED:
            return "LocalRespondingStarted";
        case ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_ENDED:
            return "LocalRespondingEnded";
        default:
            return NULL;
    }
}

static bool json_add_string(cJSON *obj, const char *key, const char *value)
{
    return obj != NULL && key != NULL && value != NULL &&
           cJSON_AddStringToObject(obj, key, value) != NULL;
}

static bool json_add_string_opt(cJSON *obj, const char *key, const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return true;
    }
    return json_add_string(obj, key, value);
}

static bool json_add_number_if_positive(cJSON *obj, const char *key, int value)
{
    if (value <= 0) {
        return true;
    }
    return obj != NULL && cJSON_AddNumberToObject(obj, key, value) != NULL;
}

static bool json_add_item(cJSON *parent, const char *key, cJSON *child)
{
    if (parent == NULL || key == NULL || child == NULL) {
        cJSON_Delete(child);
        return false;
    }
    if (!cJSON_AddItemToObject(parent, key, child)) {
        cJSON_Delete(child);
        return false;
    }
    return true;
}

static cJSON *json_create_header(const char *action, const char *task_id)
{
    cJSON *header = cJSON_CreateObject();
    if (header == NULL) {
        return NULL;
    }
    if (!json_add_string(header, "action", action) ||
        !json_add_string(header, "task_id", task_id) ||
        !json_add_string(header, "streaming", TONGYI_JSON_STREAMING_DEFAULT)) {
        cJSON_Delete(header);
        return NULL;
    }
    return header;
}

static cJSON *json_create_root(const char *action, const char *task_id, cJSON **out_payload)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *header = json_create_header(action, task_id);
    cJSON *payload = cJSON_CreateObject();
    if (root == NULL || header == NULL || payload == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(header);
        cJSON_Delete(payload);
        return NULL;
    }
    if (!json_add_item(root, "header", header)) {
        cJSON_Delete(payload);
        cJSON_Delete(root);
        return NULL;
    }
    header = NULL;
    if (!json_add_item(root, "payload", payload)) {
        cJSON_Delete(root);
        return NULL;
    }
    if (out_payload != NULL) {
        *out_payload = payload;
    }
    return root;
}

static char *json_print_and_free(cJSON *root)
{
    if (root == NULL) {
        return NULL;
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static bool json_add_object_from_string(cJSON *parent, const char *key, const char *json_str)
{
    if (json_str == NULL || json_str[0] == '\0') {
        return true;
    }
    cJSON *obj = cJSON_Parse(json_str);
    if (obj == NULL || !cJSON_IsObject(obj)) {
        ESP_LOGE(TAG, "Add JSON object failed: parse error for key '%s'", key);
        cJSON_Delete(obj);
        return false;
    }
    return json_add_item(parent, key, obj);
}

static cJSON *build_upstream(const esp_tongyi_proto_start_cfg_t *cfg)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    if (!json_add_string_opt(obj, "type", cfg->up_type) ||
        !json_add_string_opt(obj, "mode", cfg->up_mode) ||
        !json_add_string_opt(obj, "audio_format", cfg->up_audio_format) ||
        !json_add_number_if_positive(obj, "sample_rate", cfg->up_sample_rate)) {
        cJSON_Delete(obj);
        return NULL;
    }
    return obj;
}

static cJSON *build_downstream(const esp_tongyi_proto_start_cfg_t *cfg)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    if (!json_add_string_opt(obj, "voice", cfg->down_voice) ||
        !json_add_string_opt(obj, "audio_format", cfg->down_audio_format) ||
        !json_add_number_if_positive(obj, "sample_rate", cfg->down_sample_rate) ||
        !json_add_number_if_positive(obj, "frame_size", cfg->down_frame_size) ||
        !json_add_number_if_positive(obj, "volume", cfg->down_volume) ||
        !json_add_number_if_positive(obj, "bit_rate", cfg->down_bit_rate) ||
        !json_add_number_if_positive(obj, "transmit_rate_limit", cfg->down_transmit_rate_limit)) {
        cJSON_Delete(obj);
        return NULL;
    }
    return obj;
}

static cJSON *build_client_info(const esp_tongyi_proto_start_cfg_t *cfg)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    const char *user_id = (cfg->user_id != NULL && cfg->user_id[0] != '\0') ?
                          cfg->user_id : cfg->task_id;
    if (!json_add_string(obj, "user_id", user_id)) {
        cJSON_Delete(obj);
        return NULL;
    }
    if (cfg->device_uuid != NULL && cfg->device_uuid[0] != '\0') {
        cJSON *device = cJSON_CreateObject();
        if (device == NULL || !json_add_string(device, "uuid", cfg->device_uuid)) {
            cJSON_Delete(device);
            cJSON_Delete(obj);
            return NULL;
        }
        if (!json_add_item(obj, "device", device)) {
            cJSON_Delete(obj);
            return NULL;
        }
    }
    return obj;
}

static bool json_add_base64_image(cJSON *params, const char *base64_image)
{
    cJSON *imgs = cJSON_CreateArray();
    cJSON *img_item = cJSON_CreateObject();
    if (imgs == NULL || img_item == NULL) {
        cJSON_Delete(imgs);
        cJSON_Delete(img_item);
        return false;
    }
    if (!json_add_string(img_item, "type", "base64")) {
        cJSON_Delete(imgs);
        cJSON_Delete(img_item);
        return false;
    }
    cJSON *value = cJSON_CreateStringReference(base64_image);
    if (value == NULL || !cJSON_AddItemToObject(img_item, "value", value)) {
        cJSON_Delete(value);
        cJSON_Delete(imgs);
        cJSON_Delete(img_item);
        return false;
    }
    if (!cJSON_AddItemToArray(imgs, img_item)) {
        cJSON_Delete(img_item);
        cJSON_Delete(imgs);
        return false;
    }
    return json_add_item(params, "images", imgs);
}

static char *build_input_directive(const char *action, const char *task_id,
                                   const char *directive, const char *dialog_id)
{
    if (task_id == NULL || directive == NULL || dialog_id == NULL) {
        return NULL;
    }
    cJSON *payload = NULL;
    cJSON *root = json_create_root(action, task_id, &payload);
    if (root == NULL) {
        return NULL;
    }
    cJSON *input = cJSON_CreateObject();
    if (input == NULL ||
        !json_add_string(input, "directive", directive) ||
        !json_add_string(input, "dialog_id", dialog_id)) {
        cJSON_Delete(input);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "input", input)) {
        cJSON_Delete(root);
        return NULL;
    }
    return json_print_and_free(root);
}

char *esp_tongyi_proto_build_start(const esp_tongyi_proto_start_cfg_t *cfg, const char *dialog_id)
{
    if (cfg == NULL || cfg->task_id == NULL || cfg->workspace_id == NULL || cfg->app_id == NULL) {
        return NULL;
    }

    cJSON *payload = NULL;
    cJSON *root = json_create_root("run-task", cfg->task_id, &payload);
    if (root == NULL) {
        return NULL;
    }

    bool ok = json_add_string(payload, "task_group", "aigc") &&
              json_add_string(payload, "task", "multimodal-generation") &&
              json_add_string(payload, "function", "generation") &&
              json_add_string(payload, "model", "multimodal-dialog");
    if (!ok) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *input = cJSON_CreateObject();
    if (input == NULL ||
        !json_add_string(input, "directive", "Start") ||
        !json_add_string(input, "workspace_id", cfg->workspace_id) ||
        !json_add_string(input, "app_id", cfg->app_id) ||
        !json_add_string_opt(input, "dialog_id", dialog_id)) {
        cJSON_Delete(input);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "input", input)) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *params = cJSON_CreateObject();
    cJSON *upstream = build_upstream(cfg);
    cJSON *client_info = build_client_info(cfg);
    cJSON *downstream = build_downstream(cfg);
    if (params == NULL || upstream == NULL || client_info == NULL || downstream == NULL) {
        cJSON_Delete(upstream);
        cJSON_Delete(client_info);
        cJSON_Delete(downstream);
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(params, "upstream", upstream)) {
        cJSON_Delete(client_info);
        cJSON_Delete(downstream);
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(params, "client_info", client_info)) {
        cJSON_Delete(downstream);
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(params, "downstream", downstream)) {
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_object_from_string(params, "biz_params", cfg->biz_params_json)) {
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "parameters", params)) {
        cJSON_Delete(root);
        return NULL;
    }

    return json_print_and_free(root);
}

char *esp_tongyi_proto_build_stop(const char *task_id, const char *dialog_id)
{
    return build_input_directive("finish-task", task_id, "Stop", dialog_id);
}

char *esp_tongyi_proto_build_directive(const char *task_id, const char *dialog_id, const char *directive)
{
    return build_input_directive("continue-task", task_id, directive, dialog_id);
}

char *esp_tongyi_proto_build_text(const char *task_id, const char *dialog_id,
                                  const char *text, bool is_transcript)
{
    if (task_id == NULL || dialog_id == NULL || text == NULL) {
        return NULL;
    }
    cJSON *payload = NULL;
    cJSON *root = json_create_root("continue-task", task_id, &payload);
    if (root == NULL) {
        return NULL;
    }
    cJSON *input = cJSON_CreateObject();
    if (input == NULL ||
        !json_add_string(input, "directive", "RequestToRespond") ||
        !json_add_string(input, "dialog_id", dialog_id) ||
        !json_add_string(input, "type", is_transcript ? "transcript" : "prompt") ||
        !json_add_string(input, "text", text)) {
        cJSON_Delete(input);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "input", input)) {
        cJSON_Delete(root);
        return NULL;
    }
    return json_print_and_free(root);
}

char *esp_tongyi_proto_build_video_channel(const char *task_id, const char *dialog_id)
{
    if (task_id == NULL || dialog_id == NULL) {
        return NULL;
    }

    cJSON *payload = NULL;
    cJSON *root = json_create_root("continue-task", task_id, &payload);
    if (root == NULL) {
        return NULL;
    }

    cJSON *input = cJSON_CreateObject();
    if (input == NULL ||
        !json_add_string(input, "directive", "RequestToRespond") ||
        !json_add_string(input, "dialog_id", dialog_id) ||
        !json_add_string(input, "type", "prompt") ||
        !json_add_string(input, "text", "")) {
        cJSON_Delete(input);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "input", input)) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *video = cJSON_CreateObject();
    cJSON *videos = cJSON_CreateArray();
    cJSON *biz = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    if (video == NULL || videos == NULL || biz == NULL || params == NULL ||
        !json_add_string(video, "action", "connect") ||
        !json_add_string(video, "type", "voicechat_video_channel") ||
        !cJSON_AddItemToArray(videos, video)) {
        cJSON_Delete(video);
        cJSON_Delete(videos);
        cJSON_Delete(biz);
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(biz, "videos", videos) ||
        !json_add_item(params, "biz_params", biz) ||
        !json_add_item(payload, "parameters", params)) {
        cJSON_Delete(root);
        return NULL;
    }
    return json_print_and_free(root);
}

char *esp_tongyi_proto_build_image(const char *task_id, const char *dialog_id,
                                   const char *base64_image, const char *prompt,
                                   bool update_info)
{
    if (task_id == NULL || dialog_id == NULL || base64_image == NULL) {
        return NULL;
    }
    if (!update_info && prompt == NULL) {
        return NULL;
    }

    cJSON *payload = NULL;
    cJSON *root = json_create_root("continue-task", task_id, &payload);
    if (root == NULL) {
        return NULL;
    }

    cJSON *input = cJSON_CreateObject();
    bool ok = input != NULL &&
              json_add_string(input, "directive", update_info ? "UpdateInfo" : "RequestToRespond") &&
              json_add_string(input, "dialog_id", dialog_id);
    if (ok && !update_info) {
        ok = json_add_string(input, "type", "prompt") && json_add_string(input, "text", prompt);
    }
    if (!ok) {
        cJSON_Delete(input);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "input", input)) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *params = cJSON_CreateObject();
    if (params == NULL || !json_add_base64_image(params, base64_image)) {
        cJSON_Delete(params);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(payload, "parameters", params)) {
        cJSON_Delete(root);
        return NULL;
    }
    return json_print_and_free(root);
}

char *esp_tongyi_proto_build_payload_wrap(const char *task_id, const char *payload_json)
{
    if (task_id == NULL || payload_json == NULL || payload_json[0] == '\0') {
        return NULL;
    }
    cJSON *payload = cJSON_Parse(payload_json);
    if (payload == NULL || !cJSON_IsObject(payload)) {
        cJSON_Delete(payload);
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *header = json_create_header("continue-task", task_id);
    if (root == NULL || header == NULL) {
        cJSON_Delete(header);
        cJSON_Delete(payload);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(root, "header", header)) {
        cJSON_Delete(payload);
        cJSON_Delete(root);
        return NULL;
    }
    if (!json_add_item(root, "payload", payload)) {
        cJSON_Delete(root);
        return NULL;
    }
    return json_print_and_free(root);
}

static const char *extract_asr_text(cJSON *output)
{
    cJSON *txt = cJSON_GetObjectItem(output, "content");
    if (cJSON_IsString(txt) && txt->valuestring != NULL && txt->valuestring[0] != '\0') {
        return txt->valuestring;
    }
    txt = cJSON_GetObjectItem(output, "text");
    if (cJSON_IsString(txt) && txt->valuestring != NULL && txt->valuestring[0] != '\0') {
        return txt->valuestring;
    }
    return NULL;
}

static void handle_output_event(cJSON *root, const char *raw_message, esp_tongyi_proto_parse_handler_t *handler)
{
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!cJSON_IsObject(payload)) {
        return;
    }
    cJSON *output = cJSON_GetObjectItem(payload, "output");
    if (!cJSON_IsObject(output)) {
        return;
    }
    cJSON *event = cJSON_GetObjectItem(output, "event");
    if (!cJSON_IsString(event) || event->valuestring == NULL) {
        return;
    }

    const char *ev = event->valuestring;
    if (strcmp(ev, "Started") == 0) {
        cJSON *did = cJSON_GetObjectItem(output, "dialog_id");
        if (cJSON_IsString(did) && handler->on_started != NULL) {
            handler->on_started(handler->ctx, did->valuestring);
        }
    } else if (strcmp(ev, "DialogStateChanged") == 0) {
        cJSON *st = cJSON_GetObjectItem(output, "state");
        if (cJSON_IsString(st) && handler->on_dialog_state_changed != NULL) {
            handler->on_dialog_state_changed(handler->ctx, st->valuestring);
        }
    } else if (strcmp(ev, "SpeechStarted") == 0) {
        if (handler->on_speech_started != NULL) {
            handler->on_speech_started(handler->ctx);
        }
    } else if (strcmp(ev, "RespondingContent") == 0) {
        if (handler->on_responding_content != NULL) {
            handler->on_responding_content(handler->ctx, raw_message);
        }
    } else if (strcmp(ev, "RespondingStarted") == 0) {
        if (handler->on_responding_started != NULL) {
            handler->on_responding_started(handler->ctx);
        }
    } else if (strcmp(ev, "RespondingEnded") == 0) {
        if (handler->on_responding_ended != NULL) {
            handler->on_responding_ended(handler->ctx);
        }
    } else if (strcmp(ev, "SpeechContent") == 0) {
        if (handler->on_speech_content != NULL) {
            handler->on_speech_content(handler->ctx, extract_asr_text(output));
        }
    } else if (strcmp(ev, "Error") == 0) {
        ESP_LOGW(TAG, "Parse message: server error event received, len=%u",
                 (unsigned)(raw_message ? strlen(raw_message) : 0));
        if (handler->on_error != NULL) {
            handler->on_error(handler->ctx, raw_message);
        }
    }
}

esp_err_t esp_tongyi_proto_parse_message(const char *message, esp_tongyi_proto_parse_handler_t *handler)
{
    if (message == NULL || handler == NULL) {
        ESP_LOGE(TAG, "Parse message failed: message or handler is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGE(TAG, "Parse message failed: JSON parse error, len=%u", (unsigned)strlen(message));
        return ESP_FAIL;
    }

    cJSON *header = cJSON_GetObjectItem(root, "header");
    cJSON *event = header != NULL ? cJSON_GetObjectItem(header, "event") : NULL;
    if (cJSON_IsString(event) && event->valuestring != NULL) {
        if (strcmp(event->valuestring, "result-generated") == 0) {
            handle_output_event(root, message, handler);
        } else if (strcmp(event->valuestring, "task-failed") == 0) {
            ESP_LOGW(TAG, "Parse message: task-failed, len=%u", (unsigned)strlen(message));
            if (handler->on_task_failed != NULL) {
                handler->on_task_failed(handler->ctx, message);
            }
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}
