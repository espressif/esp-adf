/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_app_desc.h"

#include "adf_mem.h"
#include "esp_ota_service_source.h"
#include "esp_ota_service_version_utils.h"
#include "esp_ota_service_checker_tone_desc.h"

static const char *TAG = "OTA_CHK_TONE";

typedef struct {
    esp_ota_service_checker_t  base;

    uint32_t  desc_offset;
    uint32_t  expected_magic;

    esp_ota_service_version_policy_t  policy;

    char  current_version[32];
    char  expected_project_name[32];
    bool  have_expected_project;
} esp_ota_service_checker_tone_obj_t;

/** Read exactly @a want bytes from @a src into @a buf, or return an error. */
static esp_err_t read_exact(esp_ota_service_source_t *src, uint8_t *buf, size_t want)
{
    size_t got = 0;
    while (got < want) {
        int r = src->read(src, buf + got, (int)(want - got));
        if (r <= 0) {
            return ESP_ERR_INVALID_SIZE;
        }
        got += (size_t)r;
    }
    return ESP_OK;
}

static esp_err_t skip_bytes(esp_ota_service_source_t *src, size_t want)
{
    uint8_t scratch[64];
    while (want > 0) {
        int chunk = (int)(want > sizeof(scratch) ? sizeof(scratch) : want);
        int r = src->read(src, scratch, chunk);
        if (r <= 0) {
            return ESP_ERR_INVALID_SIZE;
        }
        want -= (size_t)r;
    }
    return ESP_OK;
}

static uint32_t u32_from_size(int64_t sz)
{
    if (sz < 0 || sz > (int64_t)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)sz;
}

static esp_err_t checker_tone_check(esp_ota_service_checker_t *self, const char *uri,
                                    esp_ota_service_source_t *source, esp_ota_service_check_result_t *result)
{
    if (!self || !uri || !source || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ota_service_checker_tone_obj_t *obj = (esp_ota_service_checker_tone_obj_t *)self;

    esp_err_t ret = source->open(source, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Source open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(result, 0, sizeof(*result));
    result->image_size = u32_from_size(source->get_size(source));

    if (obj->desc_offset > 0) {
        ret = skip_bytes(source, obj->desc_offset);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Skip %" PRIu32 " bytes before descriptor failed", obj->desc_offset);
            source->close(source);
            return ret;
        }
    }

    esp_app_desc_t incoming;
    ret = read_exact(source, (uint8_t *)&incoming, sizeof(incoming));
    source->close(source);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read esp_app_desc_t (%u bytes) failed", (unsigned)sizeof(incoming));
        return ret;
    }

    if (incoming.magic_word != obj->expected_magic) {
        ESP_LOGE(TAG, "Bad magic: got 0x%08" PRIx32 " expected 0x%08" PRIx32,
                 incoming.magic_word, obj->expected_magic);
        return ESP_ERR_INVALID_VERSION;
    }

    /* Populate result fields (version/label are NUL-padded already via memset). */
    strncpy(result->version, incoming.version, sizeof(result->version) - 1);
    strncpy(result->label, incoming.project_name, sizeof(result->label) - 1);

    /* Tone bins carry a non-IDF magic (0xF55F9876), so esp_ota_service_version_compare_descriptors()
     * cannot be used directly (it enforces ESP_APP_DESC_MAGIC_WORD). Evaluate the policy
     * manually: semver comparison via esp_ota_service_version_pack_semver() plus optional
     * project_name enforcement. */
    bool newer;
    if (obj->policy.require_higher_version) {
        uint32_t cur = esp_ota_service_version_pack_semver(obj->current_version);
        uint32_t inc = esp_ota_service_version_pack_semver(incoming.version);
        if (cur == UINT32_MAX || inc == UINT32_MAX) {
            ESP_LOGW(TAG, "Unparsable semver: current='%s' incoming='%s'",
                     obj->current_version, incoming.version);
            newer = false;
        } else {
            newer = (inc > cur);
        }
    } else {
        /* No strict ordering required: accept any non-empty version. */
        newer = (incoming.version[0] != '\0');
    }

    if (newer && obj->policy.check_project_name && obj->have_expected_project) {
        if (strncmp(incoming.project_name, obj->expected_project_name,
                    sizeof(incoming.project_name)) != 0) {
            ESP_LOGW(TAG, "Project name mismatch: expected='%s' incoming='%s'",
                     obj->expected_project_name, incoming.project_name);
            newer = false;
        }
    }

    result->upgrade_available = newer;

    ESP_LOGI(TAG, "Tone-desc check: incoming version='%s' project='%s' size=%" PRIu32 " upgrade_available=%s",
             result->version, result->label, result->image_size,
             result->upgrade_available ? "true" : "false");
    return ESP_OK;
}

static void checker_tone_destroy(esp_ota_service_checker_t *self)
{
    if (self) {
        adf_free(self);
    }
}

esp_ota_service_checker_t *esp_ota_service_checker_tone_desc_create(const esp_ota_service_checker_tone_desc_cfg_t *cfg)
{
    if (!cfg || !cfg->current_version || cfg->current_version[0] == '\0') {
        ESP_LOGE(TAG, "Invalid config: current_version is required");
        return NULL;
    }

    esp_ota_service_checker_tone_obj_t *obj = adf_calloc(1, sizeof(*obj));
    if (!obj) {
        ESP_LOGE(TAG, "No memory for tone-desc checker");
        return NULL;
    }

    obj->base.check = checker_tone_check;
    obj->base.destroy = checker_tone_destroy;
    obj->base.priv = NULL;

    obj->desc_offset = cfg->desc_offset > 0 ? cfg->desc_offset : ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET;
    obj->expected_magic = cfg->expected_magic > 0 ? cfg->expected_magic : ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_MAGIC;
    obj->policy = cfg->policy;

    strncpy(obj->current_version, cfg->current_version, sizeof(obj->current_version) - 1);

    if (cfg->expected_project_name && cfg->expected_project_name[0] != '\0') {
        strncpy(obj->expected_project_name, cfg->expected_project_name,
                sizeof(obj->expected_project_name) - 1);
        obj->have_expected_project = true;
    }

    return &obj->base;
}
