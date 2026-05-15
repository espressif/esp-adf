/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"

#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_profile_mgr.h"

#define ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION  (1u)

/**
 * @brief  Configuration image for esp_config_manager
 */
typedef struct __attribute__((packed)) {
    uint32_t                    schema_version;    /*!< Must match or migrate via merge */
    uint8_t                     profile_count;     /*!< Number of valid entries in @c profiles */
    uint8_t                     last_working_idx;  /*!< 0xFF if unknown */
    esp_wifi_service_profile_t  profiles[0];       /*!< Array of profiles (variable size) */
} esp_wifi_service_profile_image_t;

/**
 * @brief  Profile manager instance (config manager + in-memory store)
 */
typedef struct {
    esp_config_manager_handle_t       cfg_mgr;             /*!< esp_config_manager handle */
    esp_wifi_service_profile_image_t *image;               /*!< Working copy of config image */
    esp_wifi_service_profile_image_t *default_image;       /*!< Default config image */
    size_t                            default_image_size;  /*!< Bytes in @c image */
    uint8_t                           max_profiles;        /*!< Runtime profile capacity */
    SemaphoreHandle_t                 lock;                /*!< Protects image and save */
} wifi_profile_manager_ctx_t;

/**
 * @brief  Context for ::wifi_profile_set_enabled_proc
 */
typedef struct {
    const char *ssid;     /*!< SSID to match */
    bool        enabled;  /*!< New enabled flag */
} wifi_profile_set_enabled_ctx_t;

/**
 * @brief  Context for ::wifi_profile_set_last_working_proc
 */
typedef struct {
    const char *ssid;  /*!< SSID to record, or NULL/empty to clear */
} wifi_profile_set_last_working_ctx_t;

/**
 * @brief  Context for ::wifi_profile_add_proc
 */
typedef struct {
    const esp_wifi_service_profile_t *profile;  /*!< Profile to add or update */
} wifi_profile_add_ctx_t;

/**
 * @brief  Context for SSID-based delete
 */
typedef struct {
    const char *ssid;  /*!< SSID to remove */
} wifi_profile_delete_ssid_ctx_t;

static const char *TAG = "WIFI_PROFILE";

static bool wifi_profile_image_size(uint8_t max_profiles, size_t *size_out)
{
    if (max_profiles == 0 || !size_out) {
        return false;
    }
    const size_t header_size = offsetof(esp_wifi_service_profile_image_t, profiles);
    if ((SIZE_MAX - header_size) / sizeof(esp_wifi_service_profile_t) < max_profiles) {
        return false;
    }
    *size_out = header_size + (size_t)max_profiles * sizeof(esp_wifi_service_profile_t);
    return true;
}

static void wifi_profile_image_set_defaults(esp_wifi_service_profile_image_t *image, size_t image_size)
{
    memset(image, 0, image_size);
    image->schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION;
    image->last_working_idx = 0xFF;
}

static void wifi_profile_normalize_image(esp_wifi_service_profile_image_t *image, uint8_t max_profiles)
{
    if (image->profile_count > max_profiles) {
        image->profile_count = max_profiles;
    }
    if (image->last_working_idx != 0xFF && image->last_working_idx >= image->profile_count) {
        image->last_working_idx = 0xFF;
    }
    image->schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION;
}

static esp_err_t wifi_profile_process_with_store(esp_wifi_service_profile_mgr_t handle,
                                                 esp_err_t (*proc)(wifi_profile_manager_ctx_t *pm, void *ctx),
                                                 void *ctx)
{
    ESP_RETURN_ON_FALSE(handle && proc, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_manager_ctx_t *profile_manager = handle;
    xSemaphoreTake(profile_manager->lock, portMAX_DELAY);
    esp_err_t ret = proc(profile_manager, ctx);
    if (ret == ESP_OK) {
        ret = esp_config_manager_save(profile_manager->cfg_mgr, profile_manager->image, (int)profile_manager->default_image_size);
    }
    xSemaphoreGive(profile_manager->lock);
    return ret;
}

static inline void set_profile_enabled(esp_wifi_service_profile_t *profile, bool enabled)
{
    if (enabled) {
        profile->flags |= ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED;
    } else {
        profile->flags = (uint8_t)(profile->flags & (uint8_t)~ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED);
    }
}

static int wifi_profile_find_by_ssid(const esp_wifi_service_profile_image_t *st, uint8_t max_profiles, const char *ssid)
{
    for (unsigned i = 0; i < st->profile_count && i < max_profiles; ++i) {
        if (strcmp(st->profiles[i].ssid, ssid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static esp_err_t wifi_profile_set_enabled_proc(wifi_profile_manager_ctx_t *pm, void *vctx)
{
    esp_wifi_service_profile_image_t *st = pm->image;
    const wifi_profile_set_enabled_ctx_t *sc = vctx;
    int idx = wifi_profile_find_by_ssid(st, pm->max_profiles, sc->ssid);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    set_profile_enabled(&st->profiles[idx], sc->enabled);
    return ESP_OK;
}

static esp_err_t wifi_profile_set_last_working_proc(wifi_profile_manager_ctx_t *pm, void *vctx)
{
    esp_wifi_service_profile_image_t *st = pm->image;
    const wifi_profile_set_last_working_ctx_t *sc = vctx;
    if (!sc->ssid || sc->ssid[0] == '\0') {
        st->last_working_idx = 0xFF;
        return ESP_OK;
    }
    int idx = wifi_profile_find_by_ssid(st, pm->max_profiles, sc->ssid);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    st->last_working_idx = (uint8_t)idx;
    return ESP_OK;
}

static esp_err_t wifi_profile_add_proc(wifi_profile_manager_ctx_t *pm, void *vctx)
{
    esp_wifi_service_profile_image_t *st = pm->image;
    wifi_profile_add_ctx_t *ac = vctx;
    const esp_wifi_service_profile_t *profile_in = ac->profile;
    const size_t ssid_len = strnlen(profile_in->ssid, ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1);
    if (ssid_len == 0 || ssid_len > ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (profile_in->priority > ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t plen = strnlen(profile_in->password, ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN + 1);
    if (plen > ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_wifi_service_profile_t profile = {0};
    profile.flags = profile_in->flags;
    profile.priority = profile_in->priority;
    strlcpy(profile.ssid, profile_in->ssid, sizeof(profile.ssid));
    strlcpy(profile.password, profile_in->password, sizeof(profile.password));

    int existing = wifi_profile_find_by_ssid(st, pm->max_profiles, profile.ssid);

    if (existing >= 0) {
        st->profiles[existing] = profile;
        st->schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION;
        return ESP_OK;
    }

    if (st->profile_count >= pm->max_profiles) {
        return ESP_ERR_NO_MEM;
    }

    const unsigned idx = st->profile_count;
    st->profiles[idx] = profile;
    st->profile_count++;
    st->schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION;
    return ESP_OK;
}

static esp_err_t wifi_profile_delete_by_ssid_proc(wifi_profile_manager_ctx_t *pm, void *vctx)
{
    esp_wifi_service_profile_image_t *st = pm->image;
    wifi_profile_delete_ssid_ctx_t *delete_ctx = vctx;
    int found = wifi_profile_find_by_ssid(st, pm->max_profiles, delete_ctx->ssid);
    if (found < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (st->last_working_idx == (uint8_t)found) {
        st->last_working_idx = 0xFF;
    } else if (st->last_working_idx != 0xFF && st->last_working_idx > (uint8_t)found) {
        st->last_working_idx--;
    }
    for (unsigned j = (unsigned)found; j + 1 < st->profile_count; ++j) {
        st->profiles[j] = st->profiles[j + 1];
    }
    memset(&st->profiles[st->profile_count - 1], 0, sizeof(esp_wifi_service_profile_t));
    st->profile_count--;
    st->schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION;
    return ESP_OK;
}

static esp_err_t wifi_profile_clear_all_proc(wifi_profile_manager_ctx_t *pm, void *vctx)
{
    (void)vctx;
    wifi_profile_image_set_defaults(pm->image, pm->default_image_size);
    return ESP_OK;
}

esp_err_t esp_wifi_service_profile_mgr_init(const esp_wifi_service_profile_mgr_cfg_t *cfg,
                                            esp_wifi_service_profile_mgr_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg && out_handle && cfg->storage, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(cfg->max_profiles > 0, ESP_ERR_INVALID_ARG, TAG, "max_profiles must be greater than 0");

    size_t store_sz = 0;
    ESP_RETURN_ON_FALSE(wifi_profile_image_size(cfg->max_profiles, &store_sz), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid profile capacity");
    wifi_profile_manager_ctx_t *pm = heap_caps_calloc_prefer(1, sizeof(*pm), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(pm, ESP_ERR_NO_MEM, TAG, "Allocation failed");
    pm->default_image_size = store_sz;
    pm->max_profiles = cfg->max_profiles;

    pm->image = (esp_wifi_service_profile_image_t *)heap_caps_calloc_prefer(
        1, pm->default_image_size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    pm->default_image = (esp_wifi_service_profile_image_t *)heap_caps_calloc_prefer(
        1, pm->default_image_size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!pm->image || !pm->default_image) {
        heap_caps_free(pm->image);
        heap_caps_free(pm->default_image);
        heap_caps_free(pm);
        return ESP_ERR_NO_MEM;
    }
    wifi_profile_image_set_defaults(pm->image, pm->default_image_size);
    wifi_profile_image_set_defaults(pm->default_image, pm->default_image_size);

    pm->lock = xSemaphoreCreateMutexWithCaps(ESP_WIFI_SERVICE_CAPS);
    if (!pm->lock) {
        heap_caps_free(pm->image);
        heap_caps_free(pm->default_image);
        heap_caps_free(pm);
        return ESP_ERR_NO_MEM;
    }
    const esp_config_manager_cfg_t cm = {
        .storage = cfg->storage,
        .default_config = pm->default_image,
        .default_size = store_sz,
        .schema_version = ESP_WIFI_SERVICE_PROFILE_STORE_SCHEMA_VERSION,
        .merge_fn = NULL,
        .merge_ctx = NULL,
        .crypto = cfg->crypto,
        .crypto_extra_size = cfg->crypto_extra_size,
    };

    esp_err_t ret = esp_config_manager_init(&cm, &pm->cfg_mgr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize config manager failed: %s", esp_err_to_name(ret));
        goto exit;
    }
    ret = esp_config_manager_load(pm->cfg_mgr, pm->image, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Load profile store failed: %s", esp_err_to_name(ret));
        wifi_profile_image_set_defaults(pm->image, pm->default_image_size);
    } else {
        wifi_profile_normalize_image(pm->image, pm->max_profiles);
        ESP_LOGI(TAG, "Loaded %u/%u profiles with schema version %" PRIu32,
                 (unsigned)pm->image->profile_count, (unsigned)pm->max_profiles, pm->image->schema_version);
    }
    *out_handle = pm;
    ret = ESP_OK;
exit:
    if (ret != ESP_OK) {
        esp_config_manager_deinit(pm->cfg_mgr);
        vSemaphoreDeleteWithCaps(pm->lock);
        heap_caps_free(pm->image);
        heap_caps_free(pm->default_image);
        heap_caps_free(pm);
    }
    return ret;
}

void esp_wifi_service_profile_mgr_deinit(esp_wifi_service_profile_mgr_t handle)
{
    wifi_profile_manager_ctx_t *pm = handle;
    if (!pm) {
        return;
    }
    if (pm->cfg_mgr) {
        esp_config_manager_deinit(pm->cfg_mgr);
        pm->cfg_mgr = NULL;
    }
    if (pm->lock) {
        vSemaphoreDeleteWithCaps(pm->lock);
        pm->lock = NULL;
    }
    heap_caps_free(pm->image);
    heap_caps_free(pm->default_image);
    heap_caps_free(pm);
}

esp_err_t esp_wifi_service_profile_mgr_count(esp_wifi_service_profile_mgr_t handle, uint8_t *count_out)
{
    ESP_RETURN_ON_FALSE(handle && count_out, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_manager_ctx_t *pm = handle;
    xSemaphoreTake(pm->lock, portMAX_DELAY);
    *count_out = pm->image->profile_count;
    xSemaphoreGive(pm->lock);
    return ESP_OK;
}

esp_err_t esp_wifi_service_profile_mgr_foreach(esp_wifi_service_profile_mgr_t handle,
                                               bool (*callback)(const esp_wifi_service_profile_t *profile, void *user_ctx),
                                               void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle && callback, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_manager_ctx_t *pm = handle;
    xSemaphoreTake(pm->lock, portMAX_DELAY);
    for (unsigned i = 0; i < pm->image->profile_count && i < pm->max_profiles; ++i) {
        if (!callback(&pm->image->profiles[i], user_ctx)) {
            break;
        }
    }
    xSemaphoreGive(pm->lock);
    return ESP_OK;
}

esp_err_t esp_wifi_service_profile_mgr_get(esp_wifi_service_profile_mgr_t handle, const char *ssid, esp_wifi_service_profile_t *profile_out)
{
    ESP_RETURN_ON_FALSE(handle && profile_out, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(ssid, ESP_ERR_INVALID_ARG, TAG, "ssid");
    wifi_profile_manager_ctx_t *pm = handle;
    xSemaphoreTake(pm->lock, portMAX_DELAY);
    int idx = wifi_profile_find_by_ssid(pm->image, pm->max_profiles, ssid);
    if (idx >= 0) {
        *profile_out = pm->image->profiles[idx];
    }
    xSemaphoreGive(pm->lock);
    return (idx >= 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t esp_wifi_service_profile_mgr_set_enabled(esp_wifi_service_profile_mgr_t handle, const char *ssid, bool enabled)
{
    ESP_RETURN_ON_FALSE(handle && ssid, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_set_enabled_ctx_t ctx = {
        .ssid = ssid,
        .enabled = enabled,
    };
    return wifi_profile_process_with_store(handle, wifi_profile_set_enabled_proc, &ctx);
}

esp_err_t esp_wifi_service_profile_mgr_set_last_working(esp_wifi_service_profile_mgr_t handle, const char *ssid)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    wifi_profile_set_last_working_ctx_t ctx = {
        .ssid = ssid,
    };
    return wifi_profile_process_with_store(handle, wifi_profile_set_last_working_proc, &ctx);
}

esp_err_t esp_wifi_service_profile_mgr_get_last_working(esp_wifi_service_profile_mgr_t handle, char *ssid, size_t ssid_len)
{
    ESP_RETURN_ON_FALSE(handle && ssid, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(ssid_len > ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN, ESP_ERR_INVALID_ARG, TAG, "ssid_len too small");
    wifi_profile_manager_ctx_t *pm = handle;
    xSemaphoreTake(pm->lock, portMAX_DELAY);
    const uint8_t idx = pm->image->last_working_idx;
    esp_err_t ret;
    if (idx == 0xFF || idx >= pm->image->profile_count) {
        ret = ESP_ERR_NOT_FOUND;
    } else {
        strlcpy(ssid, pm->image->profiles[idx].ssid, ssid_len);
        ret = ESP_OK;
    }
    xSemaphoreGive(pm->lock);
    return ret;
}

esp_err_t esp_wifi_service_profile_mgr_add(esp_wifi_service_profile_mgr_t handle, esp_wifi_service_profile_t *profile)
{
    ESP_RETURN_ON_FALSE(handle && profile, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_add_ctx_t ctx = {
        .profile = profile,
    };
    return wifi_profile_process_with_store(handle, wifi_profile_add_proc, &ctx);
}

esp_err_t esp_wifi_service_profile_mgr_delete(esp_wifi_service_profile_mgr_t handle, const char *ssid)
{
    ESP_RETURN_ON_FALSE(handle && ssid, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    wifi_profile_delete_ssid_ctx_t ctx = {
        .ssid = ssid,
    };
    return wifi_profile_process_with_store(handle, wifi_profile_delete_by_ssid_proc, &ctx);
}

esp_err_t esp_wifi_service_profile_mgr_clear_all(esp_wifi_service_profile_mgr_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    return wifi_profile_process_with_store(handle, wifi_profile_clear_all_proc, NULL);
}
