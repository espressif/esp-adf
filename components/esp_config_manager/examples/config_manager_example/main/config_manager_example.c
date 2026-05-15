/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/* Backend ids for EXAMPLE_CFG_STORAGE_BACKEND */
#define EXAMPLE_STORAGE_NVS    1
#define EXAMPLE_STORAGE_FS     2
#define EXAMPLE_STORAGE_FLASH  3

/* Select storage medium (edit this line). Requires partitions.csv + sdkconfig.defaults for FS/FLASH. */
#define EXAMPLE_CFG_STORAGE_BACKEND  EXAMPLE_STORAGE_NVS

/* Set to 0 to run the multi-config demo only without long automated checks. */
#define CFG_EX_RUN_SELFTEST  1

#define CFG_EX_NUM_GROUPS  3

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_config_manager.h"
#include "esp_config_storage.h"

#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
#include "esp_spiffs.h"
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS */

#if CFG_EX_RUN_SELFTEST && EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
#include "esp_partition.h"
#endif  /* CFG_EX_RUN_SELFTEST && EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH */

#if CFG_EX_RUN_SELFTEST && EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
#include "nvs.h"
#endif  /* CFG_EX_RUN_SELFTEST && EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */

static const char *TAG = "CFG_MANAGER_EXAMPLE";

/**
 * @brief  Group 0: audio / UI (larger blob; merge-tail demo in selftest)
 */
typedef struct __attribute__((packed)) {
    uint32_t  volume_percent;    /*!< Volume 0–100 */
    uint32_t  feature_flags;     /*!< Feature bitmask */
    char      profile_name[16];  /*!< NUL-terminated label */
    uint32_t  brightness;        /*!< UI brightness */
    uint8_t   volume_level;      /*!< Discrete level */
    uint8_t   array[5];          /*!< Demo byte array */
} example_audio_config_t;

static const example_audio_config_t s_defaults_audio = {
    .volume_percent = 42,
    .feature_flags  = 1,
    .profile_name   = "default",
    .brightness     = 80,
    .volume_level   = 10,
    .array          = "esp1",
};

/**
 * @brief  Group 1: lifecycle / counters (small blob)
 */
typedef struct __attribute__((packed)) {
    uint32_t  boot_count;     /*!< Power-on counter */
    uint16_t  session_flags;  /*!< Session flags */
    uint8_t   reserved;       /*!< Padding / future use */
    uint8_t   array[5];       /*!< Demo byte array */
} example_lifecycle_config_t;

static const example_lifecycle_config_t s_defaults_lifecycle = {
    .boot_count    = 0,
    .session_flags = 0x100,
    .reserved      = 0,
    .array         = "esp2",
};

/**
 * @brief  Group 2: network stub (first u32 used by generic round-trip selftest)
 */
typedef struct __attribute__((packed)) {
    uint32_t  route_tag;  /*!< Arbitrary route id */
    char      ifname[8];  /*!< Interface name */
    uint16_t  mtu;        /*!< MTU in bytes */
} example_net_config_t;

static const example_net_config_t s_defaults_net = {
    .route_tag = 0,
    .ifname    = "sta0",
    .mtu       = 1500,
};

/**
 * Per-group storage (isolated namespaces / paths / partitions)
 */

#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
static esp_config_storage_nvs_t s_nvs_groups[CFG_EX_NUM_GROUPS] = {
    {.nvs_namespace = "cfg_g1", .key_primary = "main", .key_backup = "bak"},
    {.nvs_namespace = "cfg_g2", .key_primary = "main", .key_backup = "bak"},
    {.nvs_namespace = "cfg_g3", .key_primary = "main", .key_backup = "bak"},
};
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
static esp_config_storage_fs_t s_fs_groups[CFG_EX_NUM_GROUPS] = {
    {.path_primary = "/spiffs/g1_pri.bin", .path_backup = "/spiffs/g1_bak.bin"},
    {.path_primary = "/spiffs/g2_pri.bin", .path_backup = "/spiffs/g2_bak.bin"},
    {.path_primary = "/spiffs/g3_pri.bin", .path_backup = "/spiffs/g3_bak.bin"},
};
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
static esp_config_storage_flash_t s_flash_groups[CFG_EX_NUM_GROUPS] = {
    {.label_primary = "cfg_pri", .label_backup = "cfg_bak"},
    {.label_primary = "cfg2_pri", .label_backup = "cfg2_bak"},
    {.label_primary = "cfg3_pri", .label_backup = "cfg3_bak"},
};
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */

/**
 * @brief  Descriptor for one example configuration group
 */
typedef struct {
    const char *name;               /*!< Short name for logging */
    const void *defaults;           /*!< Default struct pointer */
    size_t      default_size;       /*!< sizeof default image */
    uint32_t    schema_version;     /*!< Stored schema id */
    size_t      crypto_extra_size;  /*!< Extra bytes encrypt may add versus packed plaintext record */
} example_cfg_group_desc_t;

static const example_cfg_group_desc_t s_group_desc[CFG_EX_NUM_GROUPS] = {
    {"audio", &s_defaults_audio, sizeof(example_audio_config_t), 2, 0},
    {"lifecycle", &s_defaults_lifecycle, sizeof(example_lifecycle_config_t), 11, 0},
    {"net", &s_defaults_net, sizeof(example_net_config_t), 12, 0},
};

static esp_err_t example_encrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size,
                                 size_t *out_len)
{
    (void)ctx;
    if (out_size < in_len) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(out, in, in_len);
    *out_len = in_len;
    return ESP_OK;
}

static esp_err_t example_decrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size,
                                 size_t *out_len)
{
    return example_encrypt(ctx, in, in_len, out, out_size, out_len);
}

#if !CFG_EX_RUN_SELFTEST
static void print_audio(const example_audio_config_t *c, const esp_config_load_info_t *info)
{
    const char *src = "unknown";
    if (info) {
        switch (info->source) {
            case ESP_CONFIG_LOAD_SOURCE_PRIMARY:
                src = "primary";
                break;
            case ESP_CONFIG_LOAD_SOURCE_BACKUP:
                src = "backup";
                break;
            case ESP_CONFIG_LOAD_SOURCE_DEFAULTS:
                src = "defaults";
                break;
            default:
                break;
        }
    }
    ESP_LOGI(TAG, "[%s] from %s repair=%d", "audio", src, info ? info->primary_repair_scheduled : 0);
    ESP_LOGI(TAG, "  vol=%" PRIu32 " flags=0x%08" PRIx32 " profile=%s bri=%" PRIu32 " lvl=%" PRIu8 " array=%.5s",
             c->volume_percent, c->feature_flags, c->profile_name, c->brightness, c->volume_level, c->array);
}

static void print_lifecycle(const example_lifecycle_config_t *c, const esp_config_load_info_t *info)
{
    const char *src = "?";
    if (info) {
        if (info->source == ESP_CONFIG_LOAD_SOURCE_PRIMARY) {
            src = "primary";
        } else if (info->source == ESP_CONFIG_LOAD_SOURCE_BACKUP) {
            src = "backup";
        } else if (info->source == ESP_CONFIG_LOAD_SOURCE_DEFAULTS) {
            src = "defaults";
        }
    }
    ESP_LOGI(TAG, "[%s] from %s — boot_count=%" PRIu32 " session_flags=0x%04" PRIx16, "lifecycle", src,
             c->boot_count, c->session_flags);
    ESP_LOGI(TAG, "  array=%.5s", c->array);
}

static void print_net(const example_net_config_t *c, const esp_config_load_info_t *info)
{
    const char *src = "?";
    if (info) {
        if (info->source == ESP_CONFIG_LOAD_SOURCE_PRIMARY) {
            src = "primary";
        } else if (info->source == ESP_CONFIG_LOAD_SOURCE_BACKUP) {
            src = "backup";
        } else if (info->source == ESP_CONFIG_LOAD_SOURCE_DEFAULTS) {
            src = "defaults";
        }
    }
    ESP_LOGI(TAG, "[%s] from %s — route_tag=%" PRIu32 " if=%s mtu=%" PRIu16, "net", src, c->route_tag, c->ifname,
             c->mtu);
}
#endif  /* !CFG_EX_RUN_SELFTEST */

static const char *storage_backend_name(void)
{
#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
    return "NVS";
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    return "SPIFFS (FS)";
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
    return "raw flash partition";
#else
    return "unknown";
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */
}

static esp_err_t open_group_storage(int group_index, esp_config_storage_t *out_handle)
{
#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
    return esp_config_storage_init_nvs(&s_nvs_groups[group_index], out_handle);
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    return esp_config_storage_init_fs(&s_fs_groups[group_index], out_handle);
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
    return esp_config_storage_init_flash(&s_flash_groups[group_index], out_handle);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */
}

static void *group_storage_ctx(int group_index)
{
#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
    return &s_nvs_groups[group_index];
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    return &s_fs_groups[group_index];
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
    return &s_flash_groups[group_index];
#else
    return NULL;
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */
}

#if CFG_EX_RUN_SELFTEST

static void log_test_result(const char *id, bool pass, const char *detail)
{
    ESP_LOGI(TAG, "[TEST] %s %s%s%s", id, pass ? "PASS" : "FAIL", detail ? " — " : "", detail ? detail : "");
}

#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
static const esp_partition_t *example_flash_partition_by_label(const char *label)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
}
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH */

static esp_err_t wipe_config_storage(void *storage_ctx)
{
#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
    esp_config_storage_nvs_t *c = (esp_config_storage_nvs_t *)storage_ctx;
    nvs_handle_t h;
    esp_err_t err = nvs_open(c->nvs_namespace, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    esp_config_storage_fs_t *c = (esp_config_storage_fs_t *)storage_ctx;
    (void)unlink(c->path_primary);
    (void)unlink(c->path_backup);
    return ESP_OK;
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
    esp_config_storage_flash_t *c = (esp_config_storage_flash_t *)storage_ctx;
    const esp_partition_t *partition_primary = example_flash_partition_by_label(c->label_primary);
    const esp_partition_t *partition_backup = example_flash_partition_by_label(c->label_backup);
    if (!partition_primary || !partition_backup) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t e1 = esp_partition_erase_range(partition_primary, 0, partition_primary->size);
    esp_err_t e2 = esp_partition_erase_range(partition_backup, 0, partition_backup->size);
    return (e1 == ESP_OK && e2 == ESP_OK) ? ESP_OK : ESP_FAIL;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */
}

static esp_err_t corrupt_primary_only(void *storage_ctx)
{
#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS
    esp_config_storage_nvs_t *c = (esp_config_storage_nvs_t *)storage_ctx;
    nvs_handle_t h;
    esp_err_t err = nvs_open(c->nvs_namespace, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    static const uint8_t garbage[] = {0xde, 0xad, 0xbe, 0xef};
    err = nvs_set_blob(h, c->key_primary, garbage, sizeof(garbage));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    esp_config_storage_fs_t *c = (esp_config_storage_fs_t *)storage_ctx;
    FILE *f = fopen(c->path_primary, "wb");
    if (!f) {
        return ESP_FAIL;
    }
    static const uint8_t garbage[] = {0xde, 0xad};
    size_t w = fwrite(garbage, 1, sizeof(garbage), f);
    fclose(f);
    return (w == sizeof(garbage)) ? ESP_OK : ESP_FAIL;
#elif EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FLASH
    esp_config_storage_flash_t *c = (esp_config_storage_flash_t *)storage_ctx;
    const esp_partition_t *partition_primary = example_flash_partition_by_label(c->label_primary);
    if (!partition_primary) {
        return ESP_ERR_INVALID_STATE;
    }
    static const uint8_t garbage[] = {0x01, 0x02, 0x03};
    ESP_RETURN_ON_ERROR(esp_partition_erase_range(partition_primary, 0, partition_primary->size), TAG,
                        "erase pri");
    return esp_partition_write(partition_primary, 0, garbage, sizeof(garbage));
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_NVS */
}

static bool audio_cfg_equal(const example_audio_config_t *a, const example_audio_config_t *b)
{
    return memcmp(a, b, sizeof(example_audio_config_t)) == 0;
}

/* Full path: CRC, backup, repair, merge-tail — only for audio (group 0). */
static esp_err_t run_selftests_audio(esp_config_manager_cfg_t *mgr_template, void *storage_ctx)
{
    const char *pfx = "g0";
    char tid[48];

    snprintf(tid, sizeof(tid), "%s.packed_layout", pfx);
    log_test_result(tid, sizeof(example_audio_config_t) == 34, NULL);

    static const esp_config_crypto_ops_t crypto = {
        .encrypt = example_encrypt,
        .decrypt = example_decrypt,
        .ctx = NULL,
    };
    mgr_template->crypto = &crypto;

    esp_config_manager_handle_t mgr = NULL;
    esp_err_t err = wipe_config_storage(storage_ctx);
    snprintf(tid, sizeof(tid), "%s.wipe_storage", pfx);
    log_test_result(tid, err == ESP_OK, esp_err_to_name(err));
    if (err != ESP_OK) {
        return err;
    }

    ESP_ERROR_CHECK(esp_config_manager_init(mgr_template, &mgr));
    example_audio_config_t cfg;
    esp_config_load_info_t info;
    err = esp_config_manager_load(mgr, &cfg, &info);
    bool def_ok = (err == ESP_OK && info.source == ESP_CONFIG_LOAD_SOURCE_DEFAULTS && audio_cfg_equal(&cfg, &s_defaults_audio));
    snprintf(tid, sizeof(tid), "%s.load_defaults_after_wipe", pfx);
    log_test_result(tid, def_ok, esp_err_to_name(err));

    cfg.volume_percent = 55;
    cfg.volume_level = 7;
    err = esp_config_manager_save(mgr, &cfg, sizeof(cfg));
    if (err != ESP_OK) {
        snprintf(tid, sizeof(tid), "%s.save_load_roundtrip", pfx);
        log_test_result(tid, false, esp_err_to_name(err));
        esp_config_manager_deinit(mgr);
        return err;
    }
    err = esp_config_manager_load(mgr, &cfg, &info);
    bool rt_ok = (err == ESP_OK && cfg.volume_percent == 55 && cfg.volume_level == 7 && info.source == ESP_CONFIG_LOAD_SOURCE_PRIMARY);
    snprintf(tid, sizeof(tid), "%s.save_load_roundtrip", pfx);
    log_test_result(tid, rt_ok, esp_err_to_name(err));

    err = corrupt_primary_only(storage_ctx);
    snprintf(tid, sizeof(tid), "%s.corrupt_primary_blob", pfx);
    log_test_result(tid, err == ESP_OK, esp_err_to_name(err));

    err = esp_config_manager_load(mgr, &cfg, &info);
    bool bak_ok = (err == ESP_OK && cfg.volume_percent == 55 && cfg.volume_level == 7 && info.source == ESP_CONFIG_LOAD_SOURCE_BACKUP && info.primary_repair_scheduled);
    snprintf(tid, sizeof(tid), "%s.fallback_to_backup_crc", pfx);
    log_test_result(tid, bak_ok, esp_err_to_name(err));

    err = esp_config_manager_load(mgr, &cfg, &info);
    bool repaired = (err == ESP_OK && cfg.volume_percent == 55 && cfg.volume_level == 7 && info.source == ESP_CONFIG_LOAD_SOURCE_PRIMARY);
    snprintf(tid, sizeof(tid), "%s.primary_repaired_after_load", pfx);
    log_test_result(tid, repaired, esp_err_to_name(err));

    /* Old payload: only fields through brightness (28 bytes), no volume_level / array[5]. */
    err = wipe_config_storage(storage_ctx);
    if (err == ESP_OK) {
        const size_t old_len = offsetof(example_audio_config_t, volume_level);
        example_audio_config_t oldimg;
        memcpy(&oldimg, &s_defaults_audio, sizeof(oldimg));
        err = esp_config_manager_save(mgr, &oldimg, (int)old_len);
        if (err == ESP_OK) {
            err = esp_config_manager_load(mgr, &cfg, &info);
        }
        bool merge_ok = (err == ESP_OK && cfg.volume_level == s_defaults_audio.volume_level && memcmp(cfg.array, s_defaults_audio.array, sizeof(cfg.array)) == 0);
        snprintf(tid, sizeof(tid), "%s.merge_tail_new_fields", pfx);
        log_test_result(tid, merge_ok, merge_ok ? NULL : "tail should match defaults");
    }

    esp_config_manager_deinit(mgr);
    return ESP_OK;
}

/* Lighter checks for non-audio groups (defaults + generic u32 round-trip). */
static esp_err_t run_selftests_short_group(int group_index, esp_config_manager_cfg_t *mgr_template,
                                           void *storage_ctx)
{
    char pfx[8];
    snprintf(pfx, sizeof(pfx), "g%d", group_index);
    char tid[56];

    const example_cfg_group_desc_t *d = &s_group_desc[group_index];
    static const esp_config_crypto_ops_t crypto = {
        .encrypt = example_encrypt,
        .decrypt = example_decrypt,
        .ctx = NULL,
    };
    mgr_template->crypto = &crypto;

    ESP_ERROR_CHECK(wipe_config_storage(storage_ctx));

    uint8_t buf[64];
    ESP_RETURN_ON_FALSE(d->default_size <= sizeof(buf), ESP_ERR_INVALID_SIZE, TAG, "buf");

    esp_config_manager_handle_t mgr = NULL;
    ESP_ERROR_CHECK(esp_config_manager_init(mgr_template, &mgr));

    esp_config_load_info_t info;
    esp_err_t err = esp_config_manager_load(mgr, buf, &info);
    bool pass_def = (err == ESP_OK && info.source == ESP_CONFIG_LOAD_SOURCE_DEFAULTS && memcmp(buf, d->defaults, d->default_size) == 0);
    snprintf(tid, sizeof(tid), "%s.defaults", pfx);
    log_test_result(tid, pass_def, esp_err_to_name(err));

    memcpy(buf, d->defaults, d->default_size);
    uint32_t marker = 0xC0DE0000u ^ (uint32_t)group_index;
    memcpy(buf, &marker, sizeof(marker));
    err = esp_config_manager_save(mgr, buf, (int)d->default_size);
    err = esp_config_manager_load(mgr, buf, &info);
    uint32_t readback = 0;
    memcpy(&readback, buf, sizeof(readback));
    bool pass_rt = (err == ESP_OK && readback == marker && info.source == ESP_CONFIG_LOAD_SOURCE_PRIMARY);
    snprintf(tid, sizeof(tid), "%s.u32_roundtrip", pfx);
    log_test_result(tid, pass_rt, esp_err_to_name(err));

    esp_config_manager_deinit(mgr);
    return ESP_OK;
}

static void run_all_selftests(void)
{
    ESP_LOGI(TAG, "======== selftest start (backend=%s, %d groups) ========", storage_backend_name(),
             CFG_EX_NUM_GROUPS);

    esp_config_crypto_ops_t crypto = {
        .encrypt = example_encrypt,
        .decrypt = example_decrypt,
        .ctx = NULL,
    };

    for (int g = 0; g < CFG_EX_NUM_GROUPS; g++) {
        esp_config_storage_t storage_h = NULL;
        ESP_ERROR_CHECK(open_group_storage(g, &storage_h));
        void *ctx = group_storage_ctx(g);

        const example_cfg_group_desc_t *d = &s_group_desc[g];
        esp_config_manager_cfg_t mgr_cfg = {
            .storage = storage_h,
            .default_config = d->defaults,
            .default_size = d->default_size,
            .schema_version = d->schema_version,
            .merge_fn = NULL,
            .merge_ctx = NULL,
            .crypto = &crypto,
            .crypto_extra_size = d->crypto_extra_size,
        };

        if (g == 0) {
            (void)run_selftests_audio(&mgr_cfg, ctx);
        } else {
            (void)run_selftests_short_group(g, &mgr_cfg, ctx);
        }
        esp_config_storage_deinit(storage_h);
    }

    ESP_LOGI(TAG, "======== selftest end ========");
}

#endif  /* CFG_EX_RUN_SELFTEST */

void app_main(void)
{
    ESP_LOGI(TAG, "Storage backend: %s (%d config groups)", storage_backend_name(), CFG_EX_NUM_GROUPS);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    esp_vfs_spiffs_conf_t spiffs_cfg = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 8,
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_cfg));
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS */

    esp_config_crypto_ops_t crypto = {
        .encrypt = example_encrypt,
        .decrypt = example_decrypt,
        .ctx = NULL,
    };

#if CFG_EX_RUN_SELFTEST
    run_all_selftests();
#else
    for (int g = 0; g < CFG_EX_NUM_GROUPS; g++) {
        esp_config_storage_t storage_h = NULL;
        ESP_ERROR_CHECK(open_group_storage(g, &storage_h));
        const example_cfg_group_desc_t *d = &s_group_desc[g];

        esp_config_manager_cfg_t mgr_cfg = {
            .storage           = storage_h,
            .default_config    = d->defaults,
            .default_size      = d->default_size,
            .schema_version    = d->schema_version,
            .merge_fn          = NULL,
            .merge_ctx         = NULL,
            .crypto            = &crypto,
            .crypto_extra_size = d->crypto_extra_size,
        };

        esp_config_manager_handle_t mgr = NULL;
        ESP_ERROR_CHECK(esp_config_manager_init(&mgr_cfg, &mgr));

        esp_config_load_info_t info;
        if (g == 0) {
            example_audio_config_t cfg;
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_audio(&cfg, &info);
            cfg.volume_percent = (cfg.volume_percent + 1) % 101;
            cfg.volume_level   = (cfg.volume_level + 1) % 11;
            ESP_ERROR_CHECK(esp_config_manager_save(mgr, &cfg, sizeof(cfg)));
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_audio(&cfg, &info);
        } else if (g == 1) {
            example_lifecycle_config_t cfg;
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_lifecycle(&cfg, &info);
            cfg.boot_count++;
            ESP_ERROR_CHECK(esp_config_manager_save(mgr, &cfg, sizeof(cfg)));
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_lifecycle(&cfg, &info);
        } else {
            example_net_config_t cfg;
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_net(&cfg, &info);
            cfg.route_tag ^= 0x12345678u;
            ESP_ERROR_CHECK(esp_config_manager_save(mgr, &cfg, sizeof(cfg)));
            ESP_ERROR_CHECK(esp_config_manager_load(mgr, &cfg, &info));
            print_net(&cfg, &info);
        }

        esp_config_manager_deinit(mgr);
        esp_config_storage_deinit(storage_h);
        ESP_LOGI(TAG, "--- group %s done ---", d->name);
    }
#endif  /* CFG_EX_RUN_SELFTEST */

#if EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS
    esp_vfs_spiffs_unregister("storage");
#endif  /* EXAMPLE_CFG_STORAGE_BACKEND == EXAMPLE_STORAGE_FS */

    ESP_LOGI(TAG, "Done");
}
