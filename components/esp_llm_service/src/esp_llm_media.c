/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_gmf_oal_thread.h"
#include "esp_log.h"
#include "esp_media_track.h"
#include "esp_service_scheduler.h"
#include "sdkconfig.h"

#include "esp_llm_service_internal.h"

static const char *TAG = "ESP_LLM_MEDIA";

#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
static atomic_int s_uplink_start_calls;
static atomic_int s_fail_uplink_start_call;

void esp_llm_media_test_fail_uplink_start_call(int n)
{
    atomic_store(&s_uplink_start_calls, 0);
    atomic_store(&s_fail_uplink_start_call, n > 0 ? n : 0);
}
#endif

static const struct {
    esp_media_track_type_t  type;
    esp_llm_capability_t    capability;
    const char             *name;
} s_path_meta[ESP_LLM_MEDIA_STREAM_COUNT] = {
    [ESP_LLM_MEDIA_STREAM_AUDIO_UP] = {ESP_MEDIA_TRACK_TYPE_AUDIO, ESP_LLM_CAP_AUDIO_INPUT,  "audio uplink"},
    [ESP_LLM_MEDIA_STREAM_VIDEO_UP] = {ESP_MEDIA_TRACK_TYPE_VIDEO, ESP_LLM_CAP_VIDEO_INPUT,  "video uplink"},
    [ESP_LLM_MEDIA_STREAM_AUDIO_DN] = {ESP_MEDIA_TRACK_TYPE_AUDIO, ESP_LLM_CAP_AUDIO_OUTPUT, "audio downlink"},
    [ESP_LLM_MEDIA_STREAM_VIDEO_DN] = {ESP_MEDIA_TRACK_TYPE_VIDEO, ESP_LLM_CAP_VIDEO_OUTPUT, "video downlink"},
};

static bool path_uses_manager(const esp_llm_service_t *service, const esp_llm_media_path_t *path)
{
    (void)service;
    return path->enabled;
}

static const char *uplink_name(const esp_llm_media_path_t *path)
{
    return path->type == ESP_MEDIA_TRACK_TYPE_AUDIO ? "audio uplink" : "video uplink";
}

static uint32_t downlink_cache_size(const esp_llm_service_t *service, esp_media_track_type_t type)
{
    uint32_t size = type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
                    service->downlink.audio_cache_size : service->downlink.video_cache_size;
    if (size == 0) {
        size = type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
               ESP_LLM_AUDIO_DN_CACHE_SIZE : ESP_LLM_VIDEO_DN_CACHE_SIZE;
    }
    return size;
}

static uint16_t downlink_addr_align(esp_media_track_type_t type)
{
    return type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
           ESP_LLM_AUDIO_DN_ADDR_ALIGN : ESP_LLM_VIDEO_DN_ADDR_ALIGN;
}

esp_err_t esp_llm_media_resolve_paths(esp_llm_service_t *service)
{
    if (service == NULL || service->adapter_desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ESP_LLM_MEDIA_STREAM_COUNT; i++) {
        esp_llm_media_path_t *path = &service->paths[i];
        memset(path, 0, sizeof(*path));
        path->type = s_path_meta[i].type;
        if (i <= ESP_LLM_MEDIA_STREAM_VIDEO_UP) {
            atomic_init(&path->up.stop, false);
        }

        const esp_llm_stream_cfg_t *stream_cfg = &service->cfg.streams[i];
        if (!stream_cfg->enabled) {
            continue;
        }
        if ((service->adapter_desc->capabilities & s_path_meta[i].capability) == 0) {
            ESP_LOGE(TAG, "Resolve %s failed: adapter '%s' does not advertise this stream",
                     s_path_meta[i].name, service->adapter_desc->name);
            return ESP_ERR_NOT_SUPPORTED;
        }
        if (stream_cfg->info.type != path->type) {
            ESP_LOGE(TAG, "Resolve %s failed: cfg type %d, expected %d",
                     s_path_meta[i].name, (int)stream_cfg->info.type, (int)path->type);
            return ESP_ERR_INVALID_ARG;
        }
        if (path->type == ESP_MEDIA_TRACK_TYPE_AUDIO &&
            stream_cfg->info.info.audio.codec == 0) {
            ESP_LOGE(TAG, "Resolve %s failed: audio codec not set", s_path_meta[i].name);
            return ESP_ERR_INVALID_ARG;
        }
        if (path->type == ESP_MEDIA_TRACK_TYPE_VIDEO &&
            stream_cfg->info.info.video.codec == 0) {
            ESP_LOGE(TAG, "Resolve %s failed: video codec not set", s_path_meta[i].name);
            return ESP_ERR_INVALID_ARG;
        }
        path->info = stream_cfg->info;
        path->enabled = true;
    }
    return ESP_OK;
}

static esp_err_t wait_uplink_task(esp_llm_media_path_t *path)
{
    if (!path->up.running) {
        return ESP_OK;
    }
    if (xSemaphoreTake(path->up.done,
                       pdMS_TO_TICKS(CONFIG_ESP_LLM_SERVICE_UPLINK_STOP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Stop '%s': timed out after %d ms",
                 uplink_name(path), CONFIG_ESP_LLM_SERVICE_UPLINK_STOP_TIMEOUT_MS);
        return ESP_ERR_TIMEOUT;
    }
    path->up.running = false;
    return ESP_OK;
}

static bool uplink_send_recoverable(esp_err_t err)
{
    /* Not connected, a dropped frame, or a transient alloc/send failure.
       The session can recover without restarting this task. */
    return err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_SUPPORTED ||
           err == ESP_ERR_TIMEOUT || err == ESP_ERR_NO_MEM || err == ESP_FAIL;
}

static void uplink_consume_fatal(esp_llm_service_t *service, const char *name,
                                 const char *op, esp_err_t err)
{
    ESP_LOGE(TAG, "Consume '%s' failed: %s returned %s",
             name, op, esp_err_to_name(err));
    char buf[96];
    int n = snprintf(buf, sizeof(buf), "%s %s failed: %s", name, op, esp_err_to_name(err));
    if (n < 0) {
        n = 0;
    } else if ((size_t)n >= sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    esp_llm_event_t event = {
        .common_id = ESP_LLM_EVENT_ERROR,
        .payload_type = ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
        .role = ESP_LLM_ROLE_SYSTEM,
        .last_payload = true,
        .payload = buf,
        .payload_len = (size_t)n,
    };
    (void)esp_llm_event_dispatch(service, &event);
}

static void uplink_task(void *arg)
{
    esp_llm_media_path_t *path = (esp_llm_media_path_t *)arg;
    if (path != NULL && path->up.service != NULL) {
        esp_llm_service_t *service = path->up.service;
        const char *name = uplink_name(path);
        ESP_LOGI(TAG, "Start '%s': consume started", name);
        while (!atomic_load(&path->up.stop)) {
            esp_media_frame_t frame = {
                .track_id = path->up.track_id,
                .type = path->type,
            };
            esp_err_t ret = esp_media_provider_acquire_frame(
                &path->up.provider, &frame,
                CONFIG_ESP_LLM_SERVICE_UPLINK_FRAME_TIMEOUT_MS);
            if (ret == ESP_OK) {
                if (atomic_load(&service->uplink_send_allowed) &&
                    esp_llm_uplink_type_enabled(service, path->type) &&
                    frame.data != NULL && frame.size > 0) {
                    esp_err_t send_ret = service->adapter_desc->ops->send_frame(
                        service->adapter_ctx, &frame);
                    if (send_ret != ESP_OK && !uplink_send_recoverable(send_ret)) {
                        uplink_consume_fatal(service, name, "send", send_ret);
                        (void)esp_media_provider_release_frame(&path->up.provider, &frame);
                        break;
                    }
                }
                ret = esp_media_provider_release_frame(&path->up.provider, &frame);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Consume '%s': release returned %s",
                             name, esp_err_to_name(ret));
                }
            } else if (ret != ESP_ERR_TIMEOUT) {
                if (!atomic_load(&path->up.stop)) {
                    uplink_consume_fatal(service, name, "acquire", ret);
                }
                break;
            }
        }
        ESP_LOGI(TAG, "Stop '%s': consume stopped", name);
        xSemaphoreGive(path->up.done);
    }
    esp_gmf_oal_thread_delete(NULL);
}

static esp_err_t start_uplink_task(esp_llm_service_t *service, esp_llm_media_path_t *path)
{
    if (path->up.running) {
        ESP_LOGE(TAG, "Start uplink failed: consume is already running");
        return ESP_ERR_INVALID_STATE;
    }
#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
    int call = atomic_fetch_add(&s_uplink_start_calls, 1) + 1;
    if (atomic_load(&s_fail_uplink_start_call) == call) {
        vTaskDelay(pdMS_TO_TICKS(20));
        return ESP_ERR_NO_MEM;
    }
#endif
    if (path->up.done == NULL) {
        path->up.done = xSemaphoreCreateBinary();
        if (path->up.done == NULL) {
            ESP_LOGE(TAG, "Start uplink failed: no memory for done semaphore");
            return ESP_ERR_NO_MEM;
        }
    }
    path->up.service = service;
    atomic_store(&path->up.stop, false);
    (void)xSemaphoreTake(path->up.done, 0);

    const char *svc_name = ESP_SERVICE_BASE(service)->name;
    if (svc_name == NULL || svc_name[0] == '\0') {
        svc_name = ESP_LLM_SERVICE_NAME;
    }
    const char *thread_name = path->type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
                              ESP_LLM_SERVICE_AUDIO_UP_TASK_NAME : ESP_LLM_SERVICE_VIDEO_UP_TASK_NAME;
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_STACK,
        .priority = ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_PRIO,
        .core_id = ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_CORE,
        .is_ext = true,
    };
    esp_service_thread_cfg_t thread_cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = svc_name,
        .thread_name = thread_name,
    };
    if (esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &thread_cfg) != ESP_OK) {
        thread_cfg = default_cfg;
    }
    int8_t core_id = thread_cfg.core_id;
    if (thread_cfg.is_ext && core_id < 0) {
        core_id = 0;
    }
    if (esp_gmf_oal_thread_create(NULL, thread_name, uplink_task, path,
                                  thread_cfg.stack_size,
                                  thread_cfg.priority, thread_cfg.is_ext,
                                  core_id) != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Start uplink failed: GMF thread create failed");
        return ESP_ERR_NO_MEM;
    }
    path->up.running = true;
    return ESP_OK;
}

static void reset_downlink(esp_llm_downlink_t *dn)
{
    if (dn->mngr != NULL) {
        (void)esp_media_track_mngr_destroy(dn->mngr);
    }
    uint32_t audio_cache = dn->audio_cache_size;
    uint32_t video_cache = dn->video_cache_size;
    memset(dn, 0, sizeof(*dn));
    dn->audio_cache_size = audio_cache;
    dn->video_cache_size = video_cache;
}

esp_err_t esp_llm_media_create_downlink(esp_llm_service_t *service)
{
    uint8_t track_num = 0;
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_DN; i <= ESP_LLM_MEDIA_STREAM_VIDEO_DN; i++) {
        if (path_uses_manager(service, &service->paths[i])) {
            track_num++;
        }
    }
    if (track_num == 0) {
        return ESP_OK;
    }

    esp_media_track_mngr_cfg_t mngr_cfg = {
        .max_track_num = track_num,
    };
    esp_err_t ret = esp_media_track_mngr_create(&mngr_cfg, &service->downlink.mngr);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_DN; i <= ESP_LLM_MEDIA_STREAM_VIDEO_DN; i++) {
        esp_llm_media_path_t *path = &service->paths[i];
        if (!path_uses_manager(service, path)) {
            continue;
        }
        esp_media_track_mngr_track_cfg_t track_cfg = {
            .info = path->info,
            .cache_cfg = {
                .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
                .track_cache = {
                    .cache_size = downlink_cache_size(service, path->type),
                    .addr_align = downlink_addr_align(path->type),
                },
            },
        };
        track_cfg.info.id = path->type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
                            ESP_LLM_AUDIO_TTS_TRACK_ID : ESP_LLM_VIDEO_DOWNLINK_TRACK_ID;
        ret = esp_media_track_mngr_add_track(service->downlink.mngr, &track_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Create %s failed: %s", s_path_meta[i].name, esp_err_to_name(ret));
            esp_llm_media_destroy_downlink(service);
            return ret;
        }
    }

    ret = esp_media_track_mngr_get_provider(service->downlink.mngr, &service->downlink.provider);
    if (ret != ESP_OK) {
        esp_llm_media_destroy_downlink(service);
        return ret;
    }
    return ESP_OK;
}

void esp_llm_media_destroy_downlink(esp_llm_service_t *service)
{
    reset_downlink(&service->downlink);
}

esp_err_t esp_llm_media_prepare(esp_llm_service_t *service)
{
    if (service->downlink.mngr == NULL) {
        return ESP_OK;
    }
    return esp_media_track_clear_abort(service->downlink.mngr);
}

esp_err_t esp_llm_media_start_uplink(esp_llm_service_t *service)
{
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_UP; i <= ESP_LLM_MEDIA_STREAM_VIDEO_UP; i++) {
        esp_llm_media_path_t *path = &service->paths[i];
        if (!path->enabled || path->up.provider.ops == NULL) {
            continue;
        }
        esp_err_t ret = start_uplink_task(service, path);
        if (ret != ESP_OK) {
            (void)esp_llm_media_stop_uplink(service);
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t esp_llm_media_stop_uplink(esp_llm_service_t *service)
{
    atomic_store(&service->uplink_send_allowed, false);
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_UP; i <= ESP_LLM_MEDIA_STREAM_VIDEO_UP; i++) {
        atomic_store(&service->paths[i].up.stop, true);
    }
    esp_err_t ret = ESP_OK;
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_UP; i <= ESP_LLM_MEDIA_STREAM_VIDEO_UP; i++) {
        record_first_error(&ret, wait_uplink_task(&service->paths[i]));
    }
    return ret;
}

esp_err_t esp_llm_media_stop_downlink(esp_llm_service_t *service)
{
    if (service->downlink.mngr == NULL) {
        return ESP_OK;
    }
    return esp_media_track_write_abort(service->downlink.mngr);
}

esp_err_t esp_llm_media_write(esp_llm_service_t *service, const esp_media_frame_t *frame)
{
    esp_llm_media_stream_t stream = frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO ?
                                    ESP_LLM_MEDIA_STREAM_VIDEO_DN :
                                    ESP_LLM_MEDIA_STREAM_AUDIO_DN;
    esp_llm_media_path_t *path = &service->paths[stream];
    if (!path->enabled) {
        ESP_LOGE(TAG, "Write frame failed: %s is disabled", s_path_meta[stream].name);
        return ESP_ERR_NOT_SUPPORTED;
    }

    bool eos = (frame->flags & ESP_MEDIA_FRAME_FLAG_EOS) != 0;
    if (!eos && (frame->data == NULL || frame->size == 0)) {
        ESP_LOGE(TAG, "Write frame failed: payload is empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->pts == ESP_LLM_PTS_ABSENT) {
        ESP_LOGE(TAG, "Write frame failed: PTS is absent");
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t track_id = path->type == ESP_MEDIA_TRACK_TYPE_AUDIO ?
                        ESP_LLM_AUDIO_TTS_TRACK_ID : ESP_LLM_VIDEO_DOWNLINK_TRACK_ID;
    esp_media_frame_t out = {
        .track_id = track_id,
        .type = path->type,
        .data = eos ? NULL : frame->data,
        .size = eos ? 0 : frame->size,
        .pts = frame->pts,
        .dts = (frame->dts == ESP_LLM_PTS_ABSENT) ? frame->pts : frame->dts,
        .flags = frame->flags,
        .user_data = frame->user_data,
    };

    esp_err_t cb_ret = ESP_OK;
    bool delivered = false;
    if (service->cfg.downlink_cb != NULL) {
        cb_ret = service->cfg.downlink_cb(service->cfg.downlink_cb_ctx, &out);
        delivered = true;
        if (cb_ret != ESP_OK) {
            ESP_LOGW(TAG, "Write frame: downlink callback returned %s", esp_err_to_name(cb_ret));
        }
    }
    if (service->downlink.mngr != NULL && service->downlink.provider_exported) {
        esp_err_t wr_ret = esp_media_track_write_frame(service->downlink.mngr, &out,
                                                       ESP_LLM_MEDIA_WRITE_TIMEOUT_MS);
        if (wr_ret != ESP_OK && wr_ret != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Write frame failed: %s", esp_err_to_name(wr_ret));
        }
        return wr_ret;
    }
    if (delivered) {
        return cb_ret;
    }
    ESP_LOGE(TAG, "Write frame failed: no downlink callback or linked player");
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t media_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    if (base == NULL || out_role == NULL) {
        ESP_LOGE(TAG, "Get role failed: base or out_role is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_role = ESP_MEDIA_ROLE_SRC_SINK;
    return ESP_OK;
}

static esp_err_t resolve_ops_path(esp_llm_service_t *service,
                                  esp_media_stream_id_t stream, bool want_uplink,
                                  esp_llm_media_path_t **out_path)
{
    if (service == NULL || out_path == NULL || stream >= ESP_LLM_MEDIA_STREAM_COUNT) {
        ESP_LOGE(TAG, "Resolve stream failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_media_path_t *path = &service->paths[stream];
    if ((stream <= ESP_LLM_MEDIA_STREAM_VIDEO_UP) != want_uplink) {
        ESP_LOGE(TAG, "Resolve stream failed: stream is not %s",
                 want_uplink ? "uplink" : "downlink");
        return ESP_ERR_INVALID_ARG;
    }
    if (!path->enabled) {
        ESP_LOGW(TAG, "Resolve stream failed: %s is disabled", s_path_meta[stream].name);
        return ESP_ERR_NOT_SUPPORTED;
    }
    *out_path = path;
    return ESP_OK;
}

static esp_media_stream_id_t service_downlink_provider_stream(const esp_llm_service_t *service)
{
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_DN; i <= ESP_LLM_MEDIA_STREAM_VIDEO_DN; i++) {
        if (path_uses_manager(service, &service->paths[i])) {
            return (esp_media_stream_id_t)i;
        }
    }
    return ESP_LLM_MEDIA_STREAM_COUNT;
}

static esp_err_t media_get_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                    esp_media_provider_t *out_provider)
{
    if (out_provider == NULL) {
        ESP_LOGE(TAG, "Get provider failed: out_provider is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_t *service = (esp_llm_service_t *)base;
    esp_llm_media_path_t *path = NULL;
    esp_err_t ret = resolve_ops_path(service, stream, false, &path);
    if (ret != ESP_OK) {
        return ret;  /* helper already logged */
    }
    (void)path;
    if (service_downlink_provider_stream(service) != stream) {
        ESP_LOGW(TAG, "Get provider: use the first player-route downlink stream instead of %s",
                 s_path_meta[stream].name);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (service->downlink.provider.ops == NULL) {
        ESP_LOGE(TAG, "Get provider failed: downlink provider is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    *out_provider = service->downlink.provider;
    service->downlink.provider_exported = true;
    return ESP_OK;
}

static bool track_matches(const esp_media_track_info_t *actual,
                          const esp_media_track_info_t *expected)
{
    if (actual->type != expected->type) {
        return false;
    }
    if (actual->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        /* 0 bits means "not applicable" for compressed codecs such as G.711/OPUS. */
        bool bits_ok = actual->info.audio.bits_per_sample == expected->info.audio.bits_per_sample ||
                       actual->info.audio.bits_per_sample == 0 ||
                       expected->info.audio.bits_per_sample == 0;
        return actual->info.audio.codec == expected->info.audio.codec &&
               actual->info.audio.sample_rate == expected->info.audio.sample_rate &&
               bits_ok &&
               actual->info.audio.channel == expected->info.audio.channel;
    }
    return actual->info.video.codec == expected->info.video.codec &&
           actual->info.video.width == expected->info.video.width &&
           actual->info.video.height == expected->info.video.height &&
           actual->info.video.fps == expected->info.video.fps;
}

static esp_err_t provider_find_track(const esp_media_provider_t *provider,
                                     const esp_media_track_info_t *expected,
                                     uint16_t *out_id)
{
    uint16_t track_num = 0;
    esp_err_t ret = esp_media_provider_get_track_num(provider, &track_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Find track failed: get_track_num returned %s", esp_err_to_name(ret));
        return ret;
    }
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        ret = esp_media_provider_get_track_info(provider, i, &info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Find track failed: get_track_info returned %s", esp_err_to_name(ret));
            return ret;
        }
        if (track_matches(&info, expected)) {
            *out_id = info.id;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t media_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                    const esp_media_provider_t *provider)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(base, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Set provider failed: get state failed");
        return ESP_ERR_INVALID_STATE;
    }
    if (state == ESP_SERVICE_STATE_RUNNING || state == ESP_SERVICE_STATE_ERROR) {
        ESP_LOGE(TAG, "Set provider failed: service state is %s",
                 esp_llm_service_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_media_path_t *path = NULL;
    esp_err_t ret = resolve_ops_path((esp_llm_service_t *)base, stream, true, &path);
    if (ret != ESP_OK) {
        return ret;  /* helper already logged */
    }
    if (path->up.running) {
        ESP_LOGE(TAG, "Set provider failed: %s consume is running", s_path_meta[stream].name);
        return ESP_ERR_INVALID_STATE;
    }
    if (provider == NULL) {
        memset(&path->up.provider, 0, sizeof(path->up.provider));
        path->up.track_id = 0;
        return ESP_OK;
    }
    if (provider->ops == NULL || provider->ops->get_track_num == NULL ||
        provider->ops->get_track_info == NULL || provider->ops->acquire_frame == NULL ||
        provider->ops->release_frame == NULL) {
        ESP_LOGE(TAG, "Set provider failed: provider ops are incomplete");
        return ESP_ERR_INVALID_ARG;
    }
    ret = provider_find_track(provider, &path->info, &path->up.track_id);
    if (ret != ESP_OK) {
        const esp_media_audio_info_t *exp = &path->info.info.audio;
        ESP_LOGE(TAG, "Set %s provider failed: matching track format not found "
                 "(expected codec=0x%08" PRIx32 " rate=%" PRIu32 " bits=%u ch=%u)",
                 s_path_meta[stream].name, exp->codec, exp->sample_rate,
                 (unsigned)exp->bits_per_sample, (unsigned)exp->channel);
        return ret;
    }
    path->up.provider = *provider;
    return ESP_OK;
}

/*
 * get_provider exports the shared downlink track manager (player path).
 * set_provider installs a capture provider for an uplink consume task.
 * Audio AI (ai_src) only attaches AFE; capture frames still arrive through
 * set_provider. get_request / set_request are unused.
 */
static const esp_media_service_ops_t s_media_ops = {
    .get_role = media_get_role,
    .get_provider = media_get_provider,
    .set_provider = media_set_provider,
};

const esp_media_service_ops_t *esp_llm_media_ops(void)
{
    return &s_media_ops;
}
