/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "player_service.h"

static const char *TAG = "PLAYER";

/* Maximum time on_stop waits for the sim task to exit */
#define STOP_WAIT_MS  2000

struct player_service {
    esp_service_t      base;
    player_status_t    status;
    char              *uri;
    uint32_t           volume;
    uint32_t           position;
    uint32_t           duration;
    int                sim_actions;
    TaskHandle_t       sim_task;
    SemaphoreHandle_t  stop_sem;
    volatile bool      sim_running;
    volatile int       events_sent;
};

/* ── Payload release callback ────────────────────────────────────────── */

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static const char *player_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case PLAYER_EVT_STARTED:
            return "STARTED";
        case PLAYER_EVT_PAUSED:
            return "PAUSED";
        case PLAYER_EVT_STOPPED:
            return "STOPPED";
        case PLAYER_EVT_PROGRESS:
            return "PROGRESS";
        case PLAYER_EVT_VOLUME_CHANGED:
            return "VOLUME_CHANGED";
        case PLAYER_EVT_ERROR:
            return "ERROR";
        default:
            return NULL;
    }
}

static esp_err_t player_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_init: volume=%u, uri=%s", (unsigned)player->volume,
             player->uri ? player->uri : "NULL");
    player->status = PLAYER_STATUS_IDLE;
    player->position = 0;
    player->duration = 180000;
    return ESP_OK;
}

static esp_err_t player_on_deinit(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_deinit: releasing resources");
    if (player->uri) {
        free(player->uri);
        player->uri = NULL;
    }
    if (player->stop_sem) {
        vSemaphoreDelete(player->stop_sem);
        player->stop_sem = NULL;
    }
    return ESP_OK;
}

static void player_sim_task(void *arg)
{
    player_service_t *p = (player_service_t *)arg;
    ESP_LOGI(TAG, "[sim] Task started (%d actions)", p->sim_actions);

    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < p->sim_actions && p->sim_running; i++) {
        vTaskDelay(pdMS_TO_TICKS(300 + rand() % 500));
        if (!p->sim_running) {
            break;
        }

        int action = rand() % 5;
        switch (action) {
            case 0: {
                uint32_t vol = 10 + rand() % 91;
                player_service_set_volume(p, vol);
                ESP_LOGI(TAG, "[Sim %d/%d] Volume -> %u", i + 1, p->sim_actions, (unsigned)vol);
                break;
            }
            case 1:
                player_service_play(p);
                ESP_LOGI(TAG, "[Sim %d/%d] Play", i + 1, p->sim_actions);
                break;
            case 2: {
                esp_service_state_t state;
                esp_service_get_state(&p->base, &state);
                if (state == ESP_SERVICE_STATE_RUNNING) {
                    esp_service_pause(&p->base);
                    ESP_LOGI(TAG, "[Sim %d/%d] Pause", i + 1, p->sim_actions);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    esp_service_resume(&p->base);
                    ESP_LOGI(TAG, "[Sim %d/%d] Resume", i + 1, p->sim_actions);
                }
                break;
            }
            case 3: {
                esp_service_state_t state;
                esp_service_get_state(&p->base, &state);
                if (state == ESP_SERVICE_STATE_RUNNING) {
                    player_service_play(p);
                    ESP_LOGI(TAG, "[Sim %d/%d] Play (repeat)", i + 1, p->sim_actions);
                }
                break;
            }
            case 4: {
                /* Simulate a progress tick while playing */
                if (p->status == PLAYER_STATUS_PLAYING && p->duration > 0) {
                    p->position += 5000;
                    if (p->position > p->duration) {
                        p->position = p->duration;
                    }
                    player_progress_payload_t *prog = malloc(sizeof(*prog));
                    if (prog) {
                        prog->position_ms = p->position;
                        prog->duration_ms = p->duration;
                        prog->percent = (uint8_t)((p->position * 100) / p->duration);
                        /* Snapshot percent BEFORE publish: release_payload may fire
                         * synchronously (callback-mode delivery or early error path)
                         * and free prog before this function returns. */
                        uint8_t pct = prog->percent;
                        esp_service_publish_event(&p->base, PLAYER_EVT_PROGRESS,
                                                  prog, sizeof(*prog),
                                                  release_payload, NULL);
                        p->events_sent++;
                        ESP_LOGI(TAG, "[Sim %d/%d] Progress %u%%",
                                 i + 1, p->sim_actions, (unsigned)pct);
                    }
                }
                break;
            }
        }
    }

    ESP_LOGI(TAG, "[sim] Task done (%d events sent)", p->events_sent);

    if (p->stop_sem) {
        xSemaphoreGive(p->stop_sem);
    }
    vTaskDelete(NULL);
}

static esp_err_t player_on_start(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_start: player ready");
    player->status = PLAYER_STATUS_STOPPED;

    if (player->sim_actions > 0) {
        player->sim_running = true;
        xTaskCreate(player_sim_task, "player_sim",
                    configMINIMAL_STACK_SIZE * 2, player,
                    tskIDLE_PRIORITY + 2, &player->sim_task);
    }
    return ESP_OK;
}

static esp_err_t player_on_stop(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    player->sim_running = false;

    if (player->sim_task && player->stop_sem) {
        xSemaphoreTake(player->stop_sem, pdMS_TO_TICKS(STOP_WAIT_MS));
        player->sim_task = NULL;
    }

    player->status = PLAYER_STATUS_IDLE;
    player->position = 0;

    esp_service_publish_event(service, PLAYER_EVT_STOPPED, NULL, 0, NULL, NULL);
    player->events_sent++;

    ESP_LOGI(TAG, "On_stop: player stopped (%d events sent)", player->events_sent);
    return ESP_OK;
}

static esp_err_t player_on_pause(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_pause: paused at %u ms", (unsigned)player->position);
    if (player->status == PLAYER_STATUS_PLAYING) {
        player->status = PLAYER_STATUS_PAUSED;
    }

    player_paused_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        payload->position_ms = player->position;
        esp_service_publish_event(service, PLAYER_EVT_PAUSED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
        player->events_sent++;
    }
    return ESP_OK;
}

static esp_err_t player_on_resume(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_resume: resume from %u ms", (unsigned)player->position);
    if (player->status == PLAYER_STATUS_PAUSED) {
        player->status = PLAYER_STATUS_PLAYING;
    }

    /* Re-publish STARTED so subscribers know playback is active again */
    player_started_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        if (player->uri) {
            strncpy(payload->uri, player->uri, sizeof(payload->uri) - 1);
            payload->uri[sizeof(payload->uri) - 1] = '\0';
        } else {
            payload->uri[0] = '\0';
        }
        payload->volume = player->volume;
        esp_service_publish_event(service, PLAYER_EVT_STARTED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
        player->events_sent++;
    }
    return ESP_OK;
}

static esp_err_t player_on_lowpower_enter(esp_service_t *service)
{
    player_service_t *player = (player_service_t *)service;
    ESP_LOGI(TAG, "On_lowpower_enter: entering low power");
    if (player->status == PLAYER_STATUS_PLAYING) {
        player->status = PLAYER_STATUS_PAUSED;
    }
    return ESP_OK;
}

static esp_err_t player_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit: waking up");
    return ESP_OK;
}

static const esp_service_ops_t s_player_ops = {
    .on_init           = player_on_init,
    .on_deinit         = player_on_deinit,
    .on_start          = player_on_start,
    .on_stop           = player_on_stop,
    .on_pause          = player_on_pause,
    .on_resume         = player_on_resume,
    .on_lowpower_enter = player_on_lowpower_enter,
    .on_lowpower_exit  = player_on_lowpower_exit,
    .event_to_name     = player_event_to_name,
};

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t player_service_create(const player_service_cfg_t *cfg, player_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    player_service_t *player = calloc(1, sizeof(player_service_t));
    if (player == NULL) {
        return ESP_ERR_NO_MEM;
    }

    player->stop_sem = xSemaphoreCreateBinary();
    if (player->stop_sem == NULL) {
        free(player);
        return ESP_ERR_NO_MEM;
    }

    if (cfg) {
        player->volume = (cfg->volume <= 100) ? cfg->volume : 100;
        player->sim_actions = cfg->sim_actions;
        if (cfg->uri) {
            player->uri = strdup(cfg->uri);
        }
    } else {
        player->volume = 50;
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = "player";
    base_cfg.user_data = player;

    esp_err_t ret = esp_service_init(&player->base, &base_cfg, &s_player_ops);
    if (ret != ESP_OK) {
        if (player->uri) {
            free(player->uri);
        }
        vSemaphoreDelete(player->stop_sem);
        free(player);
        return ret;
    }

    ESP_LOGI(TAG, "Created");
    *out_svc = player;
    return ESP_OK;
}

esp_err_t player_service_destroy(player_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

esp_err_t player_service_play(player_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Play: uri=%s, volume=%u",
             svc->uri ? svc->uri : "NULL", (unsigned)svc->volume);
    svc->status = PLAYER_STATUS_PLAYING;
    svc->position = 0;

    player_started_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        if (svc->uri) {
            strncpy(payload->uri, svc->uri, sizeof(payload->uri) - 1);
            payload->uri[sizeof(payload->uri) - 1] = '\0';
        } else {
            payload->uri[0] = '\0';
        }
        payload->volume = svc->volume;
        esp_service_publish_event(&svc->base, PLAYER_EVT_STARTED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
        svc->events_sent++;
    }
    return ESP_OK;
}

esp_err_t player_service_set_volume(player_service_t *svc, uint32_t volume)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->volume = (volume <= 100) ? volume : 100;
    ESP_LOGI(TAG, "Volume set to %u", (unsigned)svc->volume);

    player_volume_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        payload->volume = svc->volume;
        esp_service_publish_event(&svc->base, PLAYER_EVT_VOLUME_CHANGED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
        svc->events_sent++;
    }
    return ESP_OK;
}

esp_err_t player_service_get_volume(const player_service_t *svc, uint32_t *out_volume)
{
    if (svc == NULL || out_volume == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_volume = svc->volume;
    return ESP_OK;
}

esp_err_t player_service_get_status(const player_service_t *svc, player_status_t *out_status)
{
    if (svc == NULL || out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_status = svc->status;
    return ESP_OK;
}

int player_service_get_events_sent(const player_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
