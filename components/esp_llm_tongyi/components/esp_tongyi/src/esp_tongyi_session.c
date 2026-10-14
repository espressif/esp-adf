/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "sdkconfig.h"

#include "esp_gmf_oal_thread.h"

#include "esp_tongyi_internal.h"

static const char *TAG = "TONGYI_SESS";

#define REQUEST_TO_SPEAK_THROTTLE_MS  1000

void esp_tongyi_session_emit(esp_tongyi_t *inst,
                             esp_tongyi_event_t event,
                             const esp_tongyi_event_data_t *event_data)
{
    if (inst == NULL || inst->cfg.event_cb == NULL || atomic_load(&inst->closing)) {
        return;
    }
    inst->cfg.event_cb(inst, event, event_data, inst->cfg.event_ctx);
}

void esp_tongyi_session_fill_start_cfg(const esp_tongyi_t *inst, esp_tongyi_proto_start_cfg_t *out)
{
    memset(out, 0, sizeof(*out));
    out->task_id = inst->cfg.task_id;
    out->workspace_id = inst->cfg.workspace_id;
    out->app_id = inst->cfg.app_id;
    out->biz_params_json = inst->cfg.biz_params_json;
    out->up_type = inst->cfg.upstream.type;
    out->up_mode = inst->cfg.upstream.mode;
    out->up_audio_format = inst->cfg.upstream.audio_format;
    out->up_sample_rate = inst->cfg.upstream.sample_rate;
    out->down_voice = inst->cfg.downstream.voice;
    out->down_audio_format = inst->cfg.downstream.audio_format;
    out->down_sample_rate = inst->cfg.downstream.sample_rate;
    out->down_frame_size = inst->cfg.downstream.frame_size;
    out->down_volume = inst->cfg.downstream.volume;
    out->down_bit_rate = inst->cfg.downstream.bit_rate;
    out->down_transmit_rate_limit = inst->cfg.downstream.transmit_rate_limit;
    out->user_id = inst->cfg.client.user_id;
    out->device_uuid = inst->cfg.client.device_uuid;
}

esp_err_t esp_tongyi_session_snapshot_dialog_id(esp_tongyi_t *inst, char *buf, size_t buf_len)
{
    if (inst == NULL || buf == NULL || buf_len == 0) {
        ESP_LOGE(TAG, "Get dialog id failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(inst->lock, portMAX_DELAY);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (inst->dialog_id != NULL) {
        size_t n = strlen(inst->dialog_id);
        if (n + 1U > buf_len) {
            ret = ESP_ERR_INVALID_SIZE;
        } else {
            memcpy(buf, inst->dialog_id, n + 1U);
            ret = ESP_OK;
        }
    }
    xSemaphoreGive(inst->lock);
    if (ret == ESP_ERR_INVALID_SIZE) {
        ESP_LOGE(TAG, "Get dialog id failed: buffer too small");
    }
    return ret;
}

esp_err_t esp_tongyi_session_send_json(esp_tongyi_t *inst, char *json)
{
    if (json == NULL) {
        ESP_LOGE(TAG, "Send JSON failed: builder returned NULL");
        return ESP_ERR_NO_MEM;
    }
    size_t len = strlen(json);
    esp_err_t ret = esp_tongyi_transport_send_text(&inst->transport, json, len);
    free(json);
    return ret;  /* transport_send_text already logged */
}

static void set_state(esp_tongyi_t *inst, esp_tongyi_state_t state)
{
    xSemaphoreTake(inst->lock, portMAX_DELAY);
    inst->state = state;
    xSemaphoreGive(inst->lock);
}

static void set_ready(esp_tongyi_t *inst, bool ready)
{
    atomic_store(&inst->ready, ready);
}

static void clear_dialog_locked(esp_tongyi_t *inst, bool clear_saved)
{
    free(inst->dialog_id);
    inst->dialog_id = NULL;
    if (clear_saved) {
        free(inst->saved_dialog_id);
        inst->saved_dialog_id = NULL;
    }
    inst->dialog_state = ESP_TONGYI_DIALOG_STATE_IDLE;
}

static esp_err_t replace_string(char **dst, const char *src)
{
    char *copy = NULL;
    if (src != NULL) {
        copy = strdup(src);
        if (copy == NULL) {
            ESP_LOGE(TAG, "Replace string failed: allocation failed");
            return ESP_ERR_NO_MEM;
        }
    }
    free(*dst);
    *dst = copy;
    return ESP_OK;
}

static void save_dialog_for_reconnect(esp_tongyi_t *inst)
{
    xSemaphoreTake(inst->lock, portMAX_DELAY);
    if (inst->dialog_id != NULL) {
        (void)replace_string(&inst->saved_dialog_id, inst->dialog_id);
    }
    xSemaphoreGive(inst->lock);
}

static const char *resume_dialog_id(esp_tongyi_t *inst)
{
    if (inst->saved_dialog_id != NULL && inst->saved_dialog_id[0] != '\0') {
        return inst->saved_dialog_id;
    }
    return inst->cfg.dialog_id;
}

static void stop_keep_alive(esp_tongyi_t *inst)
{
    if (inst->keep_alive_timer != NULL) {
        (void)esp_timer_stop(inst->keep_alive_timer);
    }
}

static void start_keep_alive(esp_tongyi_t *inst)
{
    if (inst->keep_alive_timer == NULL || inst->cfg.keep_alive_time_sec == 0) {
        return;
    }
    uint64_t keep_us = (uint64_t)inst->cfg.keep_alive_time_sec * 1000000ULL;
    (void)esp_timer_stop(inst->keep_alive_timer);
    (void)esp_timer_start_periodic(inst->keep_alive_timer, keep_us);
}

void esp_tongyi_session_schedule_reconnect(esp_tongyi_t *inst)
{
    if (inst == NULL || atomic_load(&inst->closing) || atomic_load(&inst->user_stopped) ||
        !inst->cfg.reconnect.enabled) {
        return;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&inst->reconnect_pending, &expected, true)) {
        return;
    }
    save_dialog_for_reconnect(inst);
    uint32_t delay_ms = inst->cfg.reconnect.delay_ms != 0 ?
                        inst->cfg.reconnect.delay_ms : ESP_TONGYI_DEFAULT_RECONNECT_DELAY_MS;
    if (inst->reconnect_timer != NULL) {
        (void)esp_timer_stop(inst->reconnect_timer);
        (void)esp_timer_start_once(inst->reconnect_timer, (uint64_t)delay_ms * 1000ULL);
    }
}

static void keep_alive_timer_cb(void *arg)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)arg;
    if (inst == NULL || atomic_load(&inst->closing) || !atomic_load(&inst->ready)) {
        return;
    }
    esp_tongyi_ctrl_msg_t msg = {
        .cmd = ESP_TONGYI_CTRL_HEARTBEAT,
    };
    (void)xQueueSend(inst->ctrl_queue, &msg, 0);
}

static void reconnect_timer_cb(void *arg)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)arg;
    if (inst == NULL || atomic_load(&inst->closing) || atomic_load(&inst->user_stopped)) {
        if (inst != NULL) {
            atomic_store(&inst->reconnect_pending, false);
        }
        return;
    }
    esp_tongyi_ctrl_msg_t msg = {
        .cmd = ESP_TONGYI_CTRL_RECONNECT,
    };
    if (xQueueSend(inst->ctrl_queue, &msg, 0) != pdTRUE) {
        atomic_store(&inst->reconnect_pending, false);
    }
}

static void handle_heartbeat(esp_tongyi_t *inst)
{
    if (!atomic_load(&inst->ready) || atomic_load(&inst->closing)) {
        return;
    }
    char dialog_id[ESP_TONGYI_ID_MAX];
    if (esp_tongyi_session_snapshot_dialog_id(inst, dialog_id, sizeof(dialog_id)) != ESP_OK) {
        return;
    }
    char *json = esp_tongyi_proto_build_directive(inst->cfg.task_id, dialog_id, "HeartBeat");
    (void)esp_tongyi_session_send_json(inst, json);
}

static void handle_reconnect(esp_tongyi_t *inst)
{
    atomic_store(&inst->reconnect_pending, false);
    if (atomic_load(&inst->closing) || atomic_load(&inst->user_stopped) ||
        !inst->cfg.reconnect.enabled) {
        return;
    }
    ESP_LOGI(TAG, "Reconnect 'tongyi': restarting session");
    set_state(inst, ESP_TONGYI_STATE_RECONNECT_WAIT);
    (void)esp_tongyi_session_stop(inst, false);
    if (atomic_load(&inst->closing)) {
        return;
    }
    esp_err_t ret = esp_tongyi_session_start(inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reconnect failed: %s", esp_err_to_name(ret));
        set_state(inst, ESP_TONGYI_STATE_ERROR);
        esp_tongyi_session_schedule_reconnect(inst);
    }
}

static void ctrl_thread(void *arg)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)arg;
    while (!atomic_load(&inst->closing)) {
        esp_tongyi_ctrl_msg_t msg = {0};
        if (xQueueReceive(inst->ctrl_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (msg.cmd == ESP_TONGYI_CTRL_QUIT || atomic_load(&inst->closing)) {
            break;
        }
        switch (msg.cmd) {
            case ESP_TONGYI_CTRL_HEARTBEAT:
                handle_heartbeat(inst);
                break;
            case ESP_TONGYI_CTRL_RECONNECT:
                handle_reconnect(inst);
                break;
            case ESP_TONGYI_CTRL_DIRECTIVE:
                (void)esp_tongyi_session_send_directive(inst, msg.directive, false);
                break;
            default:
                break;
        }
    }
    esp_gmf_oal_thread_t self = inst->ctrl_thread;
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_CTRL_DONE);
    (void)esp_gmf_oal_thread_delete(self);
}

esp_err_t esp_tongyi_session_ctrl_start(esp_tongyi_t *inst)
{
    inst->ctrl_queue = xQueueCreate(ESP_TONGYI_CTRL_QUEUE_LEN, sizeof(esp_tongyi_ctrl_msg_t));
    if (inst->ctrl_queue == NULL) {
        ESP_LOGE(TAG, "Create failed: control queue allocation failed");
        return ESP_ERR_NO_MEM;
    }

    if (inst->cfg.keep_alive_time_sec > 0) {
        esp_timer_create_args_t keep_args = {
            .callback = keep_alive_timer_cb,
            .arg = inst,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ty_keepalive",
        };
        esp_err_t ret = esp_timer_create(&keep_args, &inst->keep_alive_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Create failed: keep-alive timer err=%s", esp_err_to_name(ret));
            return ret;
        }
    }

    esp_timer_create_args_t recon_args = {
        .callback = reconnect_timer_cb,
        .arg = inst,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ty_reconnect",
    };
    esp_err_t ret = esp_timer_create(&recon_args, &inst->reconnect_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: reconnect timer err=%s", esp_err_to_name(ret));
        return ret;
    }

    int core_id = inst->cfg.task.core_id >= 0 ? inst->cfg.task.core_id : tskNO_AFFINITY;
    if (esp_gmf_oal_thread_create(&inst->ctrl_thread, "tongyi_ctrl", ctrl_thread, inst,
                                  ESP_TONGYI_CTRL_STACK, inst->cfg.task.priority,
                                  true, core_id) != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Create failed: control thread create failed");
        inst->ctrl_thread = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void esp_tongyi_session_ctrl_stop(esp_tongyi_t *inst)
{
    if (inst->keep_alive_timer != NULL) {
        esp_timer_stop(inst->keep_alive_timer);
        esp_timer_delete(inst->keep_alive_timer);
        inst->keep_alive_timer = NULL;
    }
    if (inst->reconnect_timer != NULL) {
        esp_timer_stop(inst->reconnect_timer);
        esp_timer_delete(inst->reconnect_timer);
        inst->reconnect_timer = NULL;
    }
    if (inst->ctrl_thread != NULL && inst->ctrl_queue != NULL) {
        esp_tongyi_ctrl_msg_t msg = { .cmd = ESP_TONGYI_CTRL_QUIT };
        (void)xQueueSendToFront(inst->ctrl_queue, &msg, pdMS_TO_TICKS(100));
        (void)xEventGroupWaitBits(inst->event_group, ESP_TONGYI_BIT_CTRL_DONE,
                                  pdTRUE, pdTRUE, pdMS_TO_TICKS(2000));
        inst->ctrl_thread = NULL;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (inst->ctrl_queue != NULL) {
        vQueueDelete(inst->ctrl_queue);
        inst->ctrl_queue = NULL;
    }
}

static esp_err_t wait_bits(esp_tongyi_t *inst, EventBits_t want, EventBits_t *out_bits)
{
    EventBits_t bits = xEventGroupWaitBits(inst->event_group,
                                           want | ESP_TONGYI_BIT_DISCONNECTED | ESP_TONGYI_BIT_FAIL,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(inst->cfg.connect_timeout_ms));
    if (out_bits != NULL) {
        *out_bits = bits;
    }
    if (atomic_load(&inst->closing)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & (ESP_TONGYI_BIT_DISCONNECTED | ESP_TONGYI_BIT_FAIL)) {
        return ESP_FAIL;
    }
    if ((bits & want) == 0) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t esp_tongyi_session_start(esp_tongyi_t *inst)
{
    if (inst == NULL) {
        return ESP_ERR_INVALID_ARG;  /* public start already logged */
    }
    if (atomic_load(&inst->closing)) {
        ESP_LOGE(TAG, "Start failed: session is closing");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(inst->session_sem, portMAX_DELAY);
    if (atomic_load(&inst->ready)) {
        xSemaphoreGive(inst->session_sem);
        return ESP_OK;
    }
    atomic_store(&inst->user_stopped, false);

    set_state(inst, ESP_TONGYI_STATE_CONNECTING);
    xEventGroupClearBits(inst->event_group,
                         ESP_TONGYI_BIT_CONNECTED | ESP_TONGYI_BIT_READY |
                         ESP_TONGYI_BIT_DISCONNECTED | ESP_TONGYI_BIT_FAIL);

    esp_err_t ret = ESP_OK;
    bool connected = false;
    ret = esp_tongyi_transport_is_connected(&inst->transport, &connected);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: query connected err=%s", esp_err_to_name(ret));
        set_state(inst, ESP_TONGYI_STATE_ERROR);
        xSemaphoreGive(inst->session_sem);
        return ret;
    }
    if (!connected) {
        ret = esp_websocket_client_start(inst->transport.client);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Start failed: WebSocket client start failed, err=%s", esp_err_to_name(ret));
            set_state(inst, ESP_TONGYI_STATE_ERROR);
            xSemaphoreGive(inst->session_sem);
            return ret;
        }
        ret = wait_bits(inst, ESP_TONGYI_BIT_CONNECTED, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Start failed: WebSocket connect err=%s", esp_err_to_name(ret));
            set_state(inst, ESP_TONGYI_STATE_ERROR);
            xSemaphoreGive(inst->session_sem);
            return ret;
        }
    }

    set_state(inst, ESP_TONGYI_STATE_STARTING);
    xEventGroupClearBits(inst->event_group, ESP_TONGYI_BIT_READY | ESP_TONGYI_BIT_FAIL);

    esp_tongyi_proto_start_cfg_t start_cfg;
    esp_tongyi_session_fill_start_cfg(inst, &start_cfg);
    const char *dialog = resume_dialog_id(inst);
    char *json = esp_tongyi_proto_build_start(&start_cfg, dialog);
    ret = esp_tongyi_session_send_json(inst, json);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: send Start JSON err=%s", esp_err_to_name(ret));
        set_state(inst, ESP_TONGYI_STATE_ERROR);
        xSemaphoreGive(inst->session_sem);
        return ret;
    }

    ret = wait_bits(inst, ESP_TONGYI_BIT_READY, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: waiting for Started err=%s", esp_err_to_name(ret));
        set_state(inst, ESP_TONGYI_STATE_ERROR);
        xSemaphoreGive(inst->session_sem);
        return ret;
    }

    start_keep_alive(inst);
    ESP_LOGI(TAG, "Start 'tongyi': session ready");
    xSemaphoreGive(inst->session_sem);
    return ESP_OK;
}

esp_err_t esp_tongyi_session_stop(esp_tongyi_t *inst, bool explicit_stop)
{
    if (inst == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (explicit_stop) {
        atomic_store(&inst->user_stopped, true);
        atomic_store(&inst->reconnect_pending, false);
        if (inst->reconnect_timer != NULL) {
            (void)esp_timer_stop(inst->reconnect_timer);
        }
    }

    xSemaphoreTake(inst->session_sem, portMAX_DELAY);
    set_ready(inst, false);
    stop_keep_alive(inst);
    set_state(inst, ESP_TONGYI_STATE_STOPPING);

    char dialog_id[ESP_TONGYI_ID_MAX];
    bool connected = false;
    (void)esp_tongyi_transport_is_connected(&inst->transport, &connected);
    if (esp_tongyi_session_snapshot_dialog_id(inst, dialog_id, sizeof(dialog_id)) == ESP_OK &&
        connected) {
        char *json = esp_tongyi_proto_build_stop(inst->cfg.task_id, dialog_id);
        (void)esp_tongyi_session_send_json(inst, json);
    }

    if (!explicit_stop || inst->cfg.disconnect_after_stop) {
        esp_tongyi_transport_close_ws(inst);
    }

    xSemaphoreTake(inst->lock, portMAX_DELAY);
    clear_dialog_locked(inst, explicit_stop);
    inst->state = ESP_TONGYI_STATE_IDLE;
    xSemaphoreGive(inst->lock);
    xEventGroupClearBits(inst->event_group, ESP_TONGYI_BIT_READY | ESP_TONGYI_BIT_CONNECTED);

    if (explicit_stop) {
        atomic_store(&inst->user_stopped, true);
        atomic_store(&inst->reconnect_pending, false);
        if (inst->reconnect_timer != NULL) {
            (void)esp_timer_stop(inst->reconnect_timer);
        }
        ESP_LOGI(TAG, "Stop 'tongyi': session stopped%s",
                 inst->cfg.disconnect_after_stop ? ", WebSocket closed" : ", WebSocket kept");
    }
    xSemaphoreGive(inst->session_sem);
    return ESP_OK;
}

esp_err_t esp_tongyi_session_send_directive(esp_tongyi_t *inst,
                                            esp_tongyi_speech_action_t directive,
                                            bool async)
{
    if (inst == NULL) {
        ESP_LOGE(TAG, "Send directive failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *name = esp_tongyi_proto_directive_name(directive);
    if (name == NULL) {
        ESP_LOGE(TAG, "Send directive failed: directive is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (async) {
        if (inst->ctrl_queue == NULL) {
            ESP_LOGE(TAG, "Post directive failed: control queue is unavailable");
            return ESP_ERR_INVALID_STATE;
        }
        esp_tongyi_ctrl_msg_t msg = {
            .cmd = ESP_TONGYI_CTRL_DIRECTIVE,
            .directive = directive,
        };
        if (xQueueSend(inst->ctrl_queue, &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Post '%s': control queue full, event dropped", name);
            return ESP_ERR_TIMEOUT;
        }
        return ESP_OK;
    }

    if (!atomic_load(&inst->ready)) {
        ESP_LOGE(TAG, "%s failed: session is not ready", name);
        return ESP_ERR_INVALID_STATE;
    }
    if (directive == ESP_TONGYI_SPEECH_ACTION_REQUEST_TO_SPEAK) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        xSemaphoreTake(inst->lock, portMAX_DELAY);
        if (now_ms - inst->last_request_to_speak_ms < REQUEST_TO_SPEAK_THROTTLE_MS) {
            xSemaphoreGive(inst->lock);
            ESP_LOGE(TAG, "Request to speak failed: throttled to once per second");
            return ESP_ERR_INVALID_STATE;
        }
        inst->last_request_to_speak_ms = now_ms;
        xSemaphoreGive(inst->lock);
    }

    char dialog_id[ESP_TONGYI_ID_MAX];
    esp_err_t ret = esp_tongyi_session_snapshot_dialog_id(inst, dialog_id, sizeof(dialog_id));
    if (ret != ESP_OK) {
        return ret;
    }
    char *json = esp_tongyi_proto_build_directive(inst->cfg.task_id, dialog_id, name);
    return esp_tongyi_session_send_json(inst, json);
}

void esp_tongyi_session_on_connected(esp_tongyi_t *inst)
{
    xEventGroupClearBits(inst->event_group, ESP_TONGYI_BIT_DISCONNECTED);
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_CONNECTED);
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_CONNECTED, NULL);
}

void esp_tongyi_session_on_disconnected(esp_tongyi_t *inst)
{
    set_ready(inst, false);
    xEventGroupClearBits(inst->event_group, ESP_TONGYI_BIT_CONNECTED | ESP_TONGYI_BIT_READY);
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_DISCONNECTED);
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_DISCONNECTED, NULL);
    if (!atomic_load(&inst->closing)) {
        xSemaphoreTake(inst->lock, portMAX_DELAY);
        bool should_reconnect = (inst->state == ESP_TONGYI_STATE_READY ||
                                 inst->state == ESP_TONGYI_STATE_STARTING ||
                                 inst->state == ESP_TONGYI_STATE_CONNECTING);
        xSemaphoreGive(inst->lock);
        if (should_reconnect) {
            set_state(inst, ESP_TONGYI_STATE_ERROR);
            esp_tongyi_session_schedule_reconnect(inst);
        }
    }
}

void esp_tongyi_session_on_error(esp_tongyi_t *inst)
{
    set_ready(inst, false);
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_FAIL);
    if (!atomic_load(&inst->closing)) {
        set_state(inst, ESP_TONGYI_STATE_ERROR);
        esp_tongyi_session_schedule_reconnect(inst);
    }
}

void esp_tongyi_session_on_assembler_error(esp_tongyi_t *inst, esp_err_t err)
{
    ESP_LOGE(TAG, "Assembler failed: %s", esp_err_to_name(err));
    esp_tongyi_event_data_t data = {0};
    data.text = esp_err_to_name(err);
    data.text_len = data.text != NULL ? strlen(data.text) : 0;
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_ERROR, &data);
}

static void json_on_started(void *ctx, const char *dialog_id)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)ctx;
    xSemaphoreTake(inst->lock, portMAX_DELAY);
    (void)replace_string(&inst->dialog_id, dialog_id);
    (void)replace_string(&inst->saved_dialog_id, dialog_id);
    inst->state = ESP_TONGYI_STATE_READY;
    xSemaphoreGive(inst->lock);
    set_ready(inst, true);
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_READY);
    ESP_LOGI(TAG, "Start 'dialog': id=%s", dialog_id != NULL ? dialog_id : "(none)");
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_SESSION_READY, NULL);
}

static void json_on_dialog_state_changed(void *ctx, const char *state_str)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)ctx;
    esp_tongyi_dialog_state_t dialog_state;
    if (strcmp(state_str, "Listening") == 0) {
        dialog_state = ESP_TONGYI_DIALOG_STATE_LISTENING;
    } else if (strcmp(state_str, "Thinking") == 0) {
        dialog_state = ESP_TONGYI_DIALOG_STATE_THINKING;
    } else if (strcmp(state_str, "Responding") == 0) {
        dialog_state = ESP_TONGYI_DIALOG_STATE_RESPONDING;
    } else if (strcmp(state_str, "Stop") == 0 || strcmp(state_str, "Idle") == 0) {
        dialog_state = ESP_TONGYI_DIALOG_STATE_IDLE;
    } else {
        ESP_LOGW(TAG, "Dialog state ignored: unknown state=%s", state_str);
        return;
    }
    inst->dialog_state = dialog_state;
    esp_tongyi_event_data_t event_data = {
        .dialog_state = dialog_state,
    };
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_DIALOG_STATE, &event_data);
}

static void json_on_speech_started(void *ctx)
{
    esp_tongyi_session_emit((esp_tongyi_t *)ctx, ESP_TONGYI_EVENT_SPEECH_STARTED, NULL);
}

static void json_on_responding_started(void *ctx)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)ctx;
    inst->dialog_state = ESP_TONGYI_DIALOG_STATE_RESPONDING;
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_RESPONDING_STARTED, NULL);
}

static void json_on_responding_ended(void *ctx)
{
    esp_tongyi_session_emit((esp_tongyi_t *)ctx, ESP_TONGYI_EVENT_RESPONDING_ENDED, NULL);
}

static void json_on_speech_content(void *ctx, const char *asr_text)
{
    esp_tongyi_event_data_t event_data = {0};
    event_data.text = asr_text;
    event_data.text_len = asr_text != NULL ? strlen(asr_text) : 0;
    ESP_LOGI(TAG, "Speech content: len=%u", (unsigned)event_data.text_len);
    esp_tongyi_session_emit((esp_tongyi_t *)ctx, ESP_TONGYI_EVENT_SPEECH_CONTENT, &event_data);
}

static void json_on_responding_content(void *ctx, const char *raw_message)
{
    esp_tongyi_event_data_t event_data = {0};
    event_data.text = raw_message;
    event_data.text_len = raw_message != NULL ? strlen(raw_message) : 0;
    ESP_LOGI(TAG, "Reply: len=%u", (unsigned)event_data.text_len);
    esp_tongyi_session_emit((esp_tongyi_t *)ctx, ESP_TONGYI_EVENT_RESPONDING_CONTENT, &event_data);
}

static void json_on_error(void *ctx, const char *raw_message)
{
    esp_tongyi_event_data_t event_data = {0};
    event_data.text = raw_message;
    event_data.text_len = raw_message != NULL ? strlen(raw_message) : 0;
    ESP_LOGW(TAG, "Error event: len=%u", (unsigned)event_data.text_len);
    esp_tongyi_session_emit((esp_tongyi_t *)ctx, ESP_TONGYI_EVENT_ERROR, &event_data);
}

static void json_on_task_failed(void *ctx, const char *raw_message)
{
    esp_tongyi_t *inst = (esp_tongyi_t *)ctx;
    json_on_error(ctx, raw_message);
    set_ready(inst, false);
    xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_FAIL);
    set_state(inst, ESP_TONGYI_STATE_ERROR);
    esp_tongyi_session_schedule_reconnect(inst);
}

void esp_tongyi_session_on_text(esp_tongyi_t *inst, const char *message)
{
    esp_tongyi_proto_parse_handler_t parse_handler = {
        .ctx = inst,
        .on_started = json_on_started,
        .on_dialog_state_changed = json_on_dialog_state_changed,
        .on_speech_started = json_on_speech_started,
        .on_responding_started = json_on_responding_started,
        .on_responding_ended = json_on_responding_ended,
        .on_speech_content = json_on_speech_content,
        .on_responding_content = json_on_responding_content,
        .on_error = json_on_error,
        .on_task_failed = json_on_task_failed,
    };
    (void)esp_tongyi_proto_parse_message(message, &parse_handler);
}

void esp_tongyi_session_on_binary(esp_tongyi_t *inst, const uint8_t *data, size_t len)
{
    esp_tongyi_event_data_t event_data = {
        .audio = data,
        .audio_len = len,
    };
    esp_tongyi_session_emit(inst, ESP_TONGYI_EVENT_AUDIO, &event_data);
}
