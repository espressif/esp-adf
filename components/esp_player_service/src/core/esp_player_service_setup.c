/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"
#include "esp_service.h"

#include "esp_gmf_pool.h"

#include "esp_player_service_defaults.h"
#include "esp_player_service_priv.h"
#include "esp_player_service_setup.h"
#include "player_out.h"

static const char *TAG = "PLAYER_SERVICE_SETUP";

static bool ps_any_slot_has_player(const esp_player_service_t *service)
{
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        if (service->streams[i].player != NULL) {
            return true;
        }
    }
    return false;
}

static void ps_destroy_default_pool(void *pool)
{
    if (pool != NULL) {
        esp_gmf_pool_deinit((esp_gmf_pool_handle_t)pool);
    }
}

static esp_err_t ps_create_default_pool(void **out_pool)
{
    if (out_pool == NULL) {
        ESP_LOGE(TAG, "Create default pool failed: out_pool is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_gmf_pool_handle_t gmf_pool = NULL;
    if (esp_gmf_pool_init(&gmf_pool) != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Create default pool failed: pool init");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = player_out_audio_register_elements(gmf_pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build the default GMF pool: %s", esp_err_to_name(ret));
        ps_destroy_default_pool(gmf_pool);
        return ret;
    }
    *out_pool = gmf_pool;
    return ESP_OK;
}

esp_err_t ps_try_default_pool(void **pool, bool *pool_owned)
{
    if (pool == NULL || pool_owned == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*pool != NULL) {
        return ESP_OK;
    }
    esp_err_t ret = ps_create_default_pool(pool);
    if (ret == ESP_OK && *pool != NULL) {
        *pool_owned = true;
    }
    return ret;
}

void ps_clear_default_pool(void **pool, bool *pool_owned)
{
    if (pool == NULL || pool_owned == NULL || !*pool_owned) {
        return;
    }
    ps_destroy_default_pool(*pool);
    *pool = NULL;
    *pool_owned = false;
}

esp_err_t esp_player_service_apply_setup(esp_player_service_t *service,
                                         const esp_player_service_setup_t *cfg)
{
    if (service == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Apply setup failed: service or cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK ||
        state != ESP_SERVICE_STATE_INITIALIZED) {
        ESP_LOGE(TAG, "Apply setup failed: service is not INITIALIZED");
        return ESP_ERR_INVALID_STATE;
    }

    /* Drop the runtime when an output is live, and also when a player already
       exists: its A/V mask was derived from the sink this call is about to
       replace, so a deferred-then-configured output must not keep it. */
    if (player_out_audio_is_open(service->out) || ps_any_slot_has_player(service)) {
        esp_err_t stop_ret = ps_stop_runtime(service);
        if (stop_ret != ESP_OK) {
            ESP_LOGE(TAG, "Apply setup failed: stop runtime: %s", esp_err_to_name(stop_ret));
            return stop_ret;
        }
    }

    service->out_fmt = cfg->fixed_out_sample_info;
    if (service->out_fmt.sample_rate == 0) {
        service->out_fmt.sample_rate = ESP_PLAYER_SERVICE_DEFAULT_SAMPLE_RATE;
    }
    if (service->out_fmt.bits_per_sample == 0) {
        service->out_fmt.bits_per_sample = ESP_PLAYER_SERVICE_DEFAULT_BITS_PER_SAMPLE;
    }
    if (service->out_fmt.channel == 0) {
        service->out_fmt.channel = ESP_PLAYER_SERVICE_DEFAULT_CHANNEL;
    }

    /* Rebuild the output binding: whoever declared the sink also declares its format. */
    player_out_audio_destroy(&service->out);
    service->configured = false;
    player_out_audio_cfg_t out_cfg = {
        .writer = cfg->out_writer,
        .writer_ctx = cfg->out_ctx,
        .codec_dev = cfg->codec_dev,
        .out_fmt = service->out_fmt,
        .max_slots = service->max_stream_num,
        .device_volume = service->output_volume,
        .pool = service->pool,
    };
    esp_err_t ret = player_out_audio_create(&out_cfg, &service->out);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply setup failed: create audio out: %s", esp_err_to_name(ret));
        return ret;
    }
    service->configured = true;

    if (player_out_audio_has_sink(service->out)) {
        ret = ps_open_audio_out(service);
        if (ret != ESP_OK) {
            player_out_audio_destroy(&service->out);
            service->configured = false;
            ESP_LOGE(TAG, "Apply setup failed: create render: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}
