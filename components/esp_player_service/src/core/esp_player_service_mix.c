/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_player.h"

#include "esp_player_service_playback.h"
#include "esp_player_service_priv.h"
#include "player_out.h"

static const char *TAG = "PLAYER_SERVICE_MIX";

/* Preparing/playing/paused hold the floor; finished/stopped/error/idle release it. */
static bool slot_holds_floor(const player_stream_slot_t *slot)
{
    if (!slot->mix.cfg_set) {
        return false;
    }
    esp_player_state_t st = ps_slot_get_play_state(slot);
    return st == ESP_PLAYER_STATE_PLAYING ||
           st == ESP_PLAYER_STATE_PREPARING ||
           st == ESP_PLAYER_STATE_PAUSED;
}

static void apply_slot_fade(player_stream_slot_t *slot, uint8_t stream, bool foreground)
{
    if (!slot->mix.cfg_set || slot->audio_slot == NULL) {
        return;
    }
    int8_t want = foreground ? 1 : 0;
    if (slot->mix.fade_state == want) {
        return;
    }
    if (player_out_slot_set_fade(slot->service->out, stream, foreground) == ESP_OK) {
        slot->mix.fade_state = want;
        return;
    }
    ESP_LOGW(TAG, "Failed to set fade on stream %u (foreground=%d)", (unsigned)stream, (int)foreground);
}

static void restore_mix_cfg(player_mix_slot_t *mix, const player_mix_slot_t *saved)
{
    mix->cfg_set = saved->cfg_set;
    mix->gain_applied = saved->gain_applied;
    mix->priority = saved->priority;
    mix->preempt_mode = saved->preempt_mode;
    mix->on_preempt = saved->on_preempt;
    mix->fade_state = saved->fade_state;
    mix->gain = saved->gain;
}

/* Mixer gain is fixed once running; program while idle. Idempotent. */
void ps_apply_pending_mix_gains(esp_player_service_t *service)
{
    if (service == NULL || !player_out_audio_is_open(service->out) || service->streams == NULL) {
        return;
    }
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (!slot->mix.cfg_set || slot->mix.gain_applied) {
            continue;
        }
        if (player_out_slot_set_gain(service->out, i, &slot->mix.gain) == ESP_OK) {
            slot->mix.gain_applied = true;
        } else {
            ESP_LOGW(TAG, "Failed to apply mixer gain on stream %u", (unsigned)i);
        }
    }
}

/* Apply fade/pause/resume/DROP for floor-holding slots. Run on timer task. */
void ps_arbitrate_preemption(esp_player_service_t *service)
{
    if (service == NULL || service->streams == NULL) {
        return;
    }
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        player_stream_slot_t *l = &service->streams[i];
        if (!slot_holds_floor(l)) {
            /* Off floor: clear any preemption we imposed. */
            l->mix.preempted = false;
            l->mix.preempt_paused = false;
            continue;
        }
        /* Higher EXCLUSIVE -> suppressed; any higher -> ducked; else foreground. */
        bool has_higher = false;
        bool has_higher_exclusive = false;
        for (uint8_t j = 0; j < service->max_stream_num; j++) {
            if (j == i) {
                continue;
            }
            player_stream_slot_t *h = &service->streams[j];
            if (!slot_holds_floor(h) || h->mix.priority <= l->mix.priority) {
                continue;
            }
            has_higher = true;
            if (h->mix.preempt_mode == ESP_PLAYER_PREEMPT_EXCLUSIVE) {
                has_higher_exclusive = true;
                break;
            }
        }
        player_mix_state_t want = has_higher_exclusive ? PS_MIX_SUPPRESSED
                                  : has_higher         ? PS_MIX_DUCKED
                                                       : PS_MIX_FOREGROUND;
        if (want == PS_MIX_SUPPRESSED) {
            l->mix.preempted = true;
            if (l->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_PAUSE && l->player != NULL &&
                ps_slot_get_play_state(l) == ESP_PLAYER_STATE_PLAYING) {
                if (esp_player_pause(l->player) == ESP_PLAYER_ERR_OK) {
                    l->mix.preempt_paused = true;
                    ESP_LOGI(TAG, "EXCLUSIVE preempt paused stream %u", (unsigned)i);
                } else {
                    ESP_LOGW(TAG, "Failed to pause stream %u for EXCLUSIVE preempt", (unsigned)i);
                }
            }
        } else {
            if (l->mix.preempt_paused && l->player != NULL) {
                if (esp_player_resume(l->player) == ESP_PLAYER_ERR_OK) {
                    l->mix.preempt_paused = false;
                    ESP_LOGI(TAG, "Resumed stream %u after EXCLUSIVE preempt", (unsigned)i);
                } else {
                    ESP_LOGW(TAG, "Failed to resume stream %u after EXCLUSIVE preempt", (unsigned)i);
                }
            }
            /* DROP recovery: restart fill so sync clock catches real-time PTS. */
            if (l->mix.preempted && l->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_DROP &&
                l->player != NULL &&
                (l->mix.source_kind == PS_SRC_FEED || l->mix.source_kind == PS_SRC_LINK)) {
                ESP_LOGI(TAG, "DROP recovery stopped stream %u", (unsigned)i);
                esp_player_stop(l->player);
            }
            l->mix.preempted = false;
            apply_slot_fade(l, i, want == PS_MIX_FOREGROUND);
        }
    }
}

/* PAUSE only for URL; DROP only for feed/link. No-op until both cfg and source known. */
esp_err_t ps_validate_preempt_source(const player_stream_slot_t *slot)
{
    if (slot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!slot->mix.cfg_set || slot->mix.source_kind == PS_SRC_NONE) {
        return ESP_OK;  /* not enough facts yet */
    }
    bool realtime = (slot->mix.source_kind == PS_SRC_FEED || slot->mix.source_kind == PS_SRC_LINK);
    if (slot->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_PAUSE && realtime) {
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_DROP && slot->mix.source_kind == PS_SRC_URL) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_set_mix_cfg(esp_player_service_t *service,
                                         esp_media_stream_id_t stream,
                                         const esp_player_mix_cfg_t *cfg)
{
    if (service == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Set mix cfg failed: service or cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->active_gain <= 0.0f || cfg->duck_gain < 0.0f || cfg->active_gain < cfg->duck_gain) {
        ESP_LOGE(TAG, "Set mix cfg failed: invalid gain values");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->preempt_mode != ESP_PLAYER_PREEMPT_COEXIST &&
        cfg->preempt_mode != ESP_PLAYER_PREEMPT_EXCLUSIVE) {
        ESP_LOGE(TAG, "Set mix cfg failed: invalid preempt mode");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->on_preempt != ESP_PLAYER_ON_PREEMPT_PAUSE &&
        cfg->on_preempt != ESP_PLAYER_ON_PREEMPT_DROP) {
        ESP_LOGE(TAG, "Set mix cfg failed: invalid on_preempt policy");
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Set mix cfg failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    /* Arbitration only ever runs on the defer task, and only a configured slot
       can hold the floor, so this call is what makes that task necessary. */
    esp_err_t defer_ret = ps_ensure_defer(service);
    if (defer_ret != ESP_OK) {
        ESP_LOGE(TAG, "Set mix cfg failed on stream %u: no arbitration task", (unsigned)stream);
        return defer_ret;
    }
    /* Source validation and gain programming both read the new values off the slot,
       so publish first and roll back if either rejects: a failed call must not
       disturb a policy that is already arbitrating. */
    const player_mix_slot_t saved = slot->mix;
    slot->mix.priority = cfg->priority;
    slot->mix.preempt_mode = cfg->preempt_mode;
    slot->mix.on_preempt = cfg->on_preempt;
    slot->mix.gain.initial_gain = cfg->duck_gain;
    slot->mix.gain.target_gain = cfg->active_gain;
    slot->mix.gain.transition_ms = cfg->transition_ms;
    slot->mix.cfg_set = true;
    slot->mix.gain_applied = false;
    slot->mix.fade_state = -1;  /* force the first fade to be applied */
    esp_err_t ret = ps_validate_preempt_source(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid preempt policy for stream %u source", (unsigned)stream);
        restore_mix_cfg(&slot->mix, &saved);
        return ret;
    }
    if (player_out_audio_is_open(service->out)) {
        ps_apply_pending_mix_gains(service);
        if (!slot->mix.gain_applied) {
            /* set_gain failed, so the mixer still holds the previous gain. */
            ESP_LOGE(TAG, "Cannot apply mix gain on stream %u: mixer already running", (unsigned)stream);
            restore_mix_cfg(&slot->mix, &saved);
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ESP_OK;
}

esp_err_t esp_player_service_get_preempt_state(esp_player_service_t *service,
                                               esp_media_stream_id_t stream,
                                               bool *out_preempted)
{
    if (service == NULL || out_preempted == NULL) {
        ESP_LOGE(TAG, "Get preempt state failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Get preempt state failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    *out_preempted = slot->mix.preempted;
    return ESP_OK;
}
