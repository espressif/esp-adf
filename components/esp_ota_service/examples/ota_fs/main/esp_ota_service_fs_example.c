/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "sdkconfig.h"

#if !defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
#include "dev_fs_fat.h"
#include "esp_board_device.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#endif  /* !defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */

#include "esp_ota_service.h"
#include "esp_ota_service_source_fs.h"
#include "esp_ota_service_target_app.h"
#include "esp_ota_service_target_data.h"
#include "esp_ota_service_checker_app.h"
#include "esp_ota_service_checker_data_version.h"
#include "esp_ota_service_checker_tone_desc.h"
#include "esp_ota_service_version_utils.h"

#include "esp_ota_service_fs_example.h"

#define FIRMWARE_FILENAME  "firmware.bin"
#define DATA_FILENAME      "userdata.bin"
#define TONE_FILENAME      "flash_tone.bin"
#define USERDATA_LABEL     "userdata"
#define FLASH_TONE_LABEL   "flash_tone"

/** Max length of <mount>/<filename> plus "file://" prefix. */
#define ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX  96

/**
 * The example demonstrates two independent data-partition upgrades, each with
 * its own checker strategy (see partitions.csv). Each bin is optional: any
 * subset of {firmware.bin, userdata.bin, flash_tone.bin} may be present on the
 * SD card, and a matching upgrade item is added only when the file exists.
 *
 *   1. userdata.bin → partition "userdata" (0x32000 bytes)
 *      Compact 4-byte packed-semver header.
 *      - Bytes [0..3]: little-endian uint32_t = esp_ota_service_version_pack_semver("x.y.z")
 *                      (major<<16 | minor<<8 | patch). Same semver encoding as
 *                      esp_app_desc_t::version.
 *      - Bytes [4..]:  application-specific payload (optional).
 *      - Checker: esp_ota_service_checker_data_version_create().
 *      - Generator: python ../../tools/gen_userdata_bin.py <x.y.z> -o userdata.bin
 *
 *   2. flash_tone.bin → partition "flash_tone" (0x32000 bytes)
 *      Upstream audio tone bin with format=1 (mk_audio_tone.py).
 *      - Bytes [0..7]:    <HHI> = tag (0x2053) | file_count | format (=1)
 *      - Bytes [8..8+256): esp_app_desc_t (magic 0xF55F9876, project_name
 *                          "ESP_TONE_BIN"); version comes from the script's -v arg.
 *      - Followed by file table / files / CRC / tail.
 *      - Checker: esp_ota_service_checker_tone_desc_create() (reads esp_app_desc_t at offset 8).
 *      - Generator (upstream esp-adf):
 *            python tools/audio_tone/mk_audio_tone.py \
 *                -f out -r resources -v 1.0.0 -F 1 -t flash_tone.bin
 */

/**
 * Read the version string embedded in the installed tone partition.
 *
 * The tone bin layout (format=1) places an esp_app_desc_t at byte offset
 * ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET from the start of the partition.  If the
 * partition is empty or has a bad magic the function returns false and the
 * caller should treat the current version as "0.0.0" to force a first-time
 * flash.
 */
static bool read_tone_partition_version(char *out, size_t out_len)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FLASH_TONE_LABEL);
    if (!part) {
        return false;
    }
    uint8_t buf[ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET + sizeof(esp_app_desc_t)];
    if (esp_partition_read(part, 0, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    const esp_app_desc_t *desc =
        (const esp_app_desc_t *)(buf + ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET);
    if (desc->magic_word != ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_MAGIC) {
        return false;
    }
    strncpy(out, desc->version, out_len - 1);
    out[out_len - 1] = '\0';
    return true;
}

#define EVT_OTA_DONE     (BIT0)
#define EVT_OTA_FAIL     (BIT1)
#define EVT_OTA_SKIP     (BIT2)
/* Set by SESSION_END when at least one item upgraded successfully.
 * Distinguishes "session finished with real updates" (→ reboot needed) from
 * "session finished but every item was skipped" (→ no reboot needed). */
#define EVT_OTA_SUCCESS  (BIT3)

static const char *TAG = "OTA_FS";

static EventGroupHandle_t s_evt;

static void on_ota_adf_event(const adf_event_t *event, void *ctx);

#if !defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
esp_err_t esp_ota_service_fs_example_init_sdcard(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init_sdcard failed: esp_board_manager_init_device_by_name returned %s", esp_err_to_name(ret));
        return ret;
    }

    dev_fs_fat_handle_t *sd = NULL;
    ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&sd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init_sdcard failed: esp_board_device_get_handle returned %s", esp_err_to_name(ret));
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ret;
    }
    if (sd == NULL) {
        ESP_LOGE(TAG, "Init_sdcard failed: FS_SDCARD handle is NULL (%s)", esp_err_to_name(ESP_OTA_SERVICE_FS_EX_ERR_SD_HANDLE_MISSING));
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ESP_OTA_SERVICE_FS_EX_ERR_SD_HANDLE_MISSING;
    }

    ESP_LOGI(TAG, "Create 'fs_sdcard': mount_point=%s", sd->mount_point ? sd->mount_point : "?");
    return ESP_OK;
}

esp_err_t esp_ota_service_fs_example_deinit_sdcard(void)
{
    esp_err_t ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Deinit_sdcard: esp_board_manager_deinit_device_by_name returned %s", esp_err_to_name(ret));
    }
    return ret;
}
#endif  /* !defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */

/** Log a 4-byte packed-semver style header from the start of @a p. */
static void log_userdata_partition(const esp_partition_t *p)
{
    uint8_t buf[32];
    esp_err_t r = esp_partition_read(p, 0, buf, sizeof(buf));
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Userdata preview failed: esp_partition_read returned %s", esp_err_to_name(r));
        return;
    }

    uint32_t ver_le;
    memcpy(&ver_le, buf, sizeof(ver_le));
    if (ver_le == UINT32_MAX) {
        ESP_LOGI(TAG, "Userdata header: erased or invalid (first u32 LE = 0xFFFFFFFF)");
    } else {
        unsigned maj = (unsigned)((ver_le >> 16) & 0xFFU);
        unsigned min = (unsigned)((ver_le >> 8) & 0xFFU);
        unsigned pat = (unsigned)(ver_le & 0xFFU);
        ESP_LOGI(TAG, "Userdata header: 4-byte packed semver (LE u32=0x%08" PRIx32 "): %u.%u.%u",
                 ver_le, maj, min, pat);
        const esp_app_desc_t *app = esp_app_get_description();
        uint32_t app_packed = esp_ota_service_version_pack_semver(app->version);
        if (app_packed != UINT32_MAX && ver_le == app_packed) {
            ESP_LOGI(TAG, "Userdata version matches running app semver '%s'", app->version);
        }
    }

    ESP_LOGI(TAG, "Userdata partition @ 0x%08" PRIx32 " size %" PRIu32 " — first %zu bytes:", p->address, p->size,
             sizeof(buf));
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, sizeof(buf), ESP_LOG_INFO);
}

static void log_flash_tone_partition(const esp_partition_t *p)
{
    uint8_t buf[ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET + sizeof(esp_app_desc_t)];
    esp_err_t r = esp_partition_read(p, 0, buf, sizeof(buf));
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Flash_tone preview failed: esp_partition_read returned %s", esp_err_to_name(r));
        return;
    }

    esp_app_desc_t desc;
    memcpy(&desc, buf + ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET, sizeof(desc));

    if (desc.magic_word == ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_MAGIC) {
        char ver[33];
        char proj[33];
        memcpy(ver, desc.version, sizeof(desc.version));
        ver[sizeof(desc.version)] = '\0';
        memcpy(proj, desc.project_name, sizeof(desc.project_name));
        proj[sizeof(desc.project_name)] = '\0';
        ESP_LOGI(TAG, "Flash_tone header: tone bin (magic 0x%08" PRIx32 "), project='%s' version='%s'",
                 desc.magic_word, proj, ver);
    } else {
        ESP_LOGI(TAG, "Flash_tone header: no valid tone esp_app_desc_t (magic 0x%08" PRIx32 ")", desc.magic_word);
    }

    ESP_LOGI(TAG, "Flash_tone partition @ 0x%08" PRIx32 " size %" PRIu32 " — first %zu bytes:", p->address, p->size,
             sizeof(buf));
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, sizeof(buf), ESP_LOG_INFO);
}

esp_err_t esp_ota_service_fs_example_log_userdata_preview(void)
{
    bool any = false;

    const esp_partition_t *pu =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, USERDATA_LABEL);
    if (pu != NULL) {
        log_userdata_partition(pu);
        any = true;
    } else {
        ESP_LOGW(TAG, "Preview: no partition labeled '%s'", USERDATA_LABEL);
    }

    const esp_partition_t *pt =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FLASH_TONE_LABEL);
    if (pt != NULL) {
        log_flash_tone_partition(pt);
        any = true;
    } else {
        ESP_LOGW(TAG, "Preview: no partition labeled '%s'", FLASH_TONE_LABEL);
    }

    return any ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static void on_ota_adf_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL || event->payload == NULL || event->payload_len < sizeof(esp_ota_service_event_t)) {
        return;
    }
    const esp_ota_service_event_t *evt = (const esp_ota_service_event_t *)event->payload;
    switch (evt->id) {
        case ESP_OTA_SERVICE_EVT_SESSION_BEGIN:
            ESP_LOGI(TAG, "[OTA] Session started");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK:
            if (evt->error == ESP_OK && !evt->ver_check.upgrade_available) {
                ESP_LOGW(TAG, "[OTA] Item %d skipped: SD image not newer", evt->item_index);
                xEventGroupSetBits(s_evt, EVT_OTA_SKIP);
            }
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_BEGIN:
            ESP_LOGI(TAG, "[OTA] Item %d begin: partition=%s", evt->item_index,
                     evt->item_label ? evt->item_label : "(app OTA slot)");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
            ESP_LOGI(TAG, "[OTA] Progress: %" PRIu32 " / %" PRIu32 " bytes",
                     evt->progress.bytes_written, evt->progress.total_bytes);
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_END:
            switch (evt->item_end.status) {
                case ESP_OTA_SERVICE_ITEM_STATUS_OK:
                    ESP_LOGI(TAG, "[OTA] Item %d success", evt->item_index);
                    break;
                case ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED:
                    ESP_LOGW(TAG, "[OTA] Item %d skipped (reason=%d)",
                             evt->item_index, (int)evt->item_end.reason);
                    xEventGroupSetBits(s_evt, EVT_OTA_SKIP);
                    break;
                case ESP_OTA_SERVICE_ITEM_STATUS_FAILED:
                    ESP_LOGE(TAG, "[OTA] Item %d failed: %s (reason=%d)", evt->item_index,
                             esp_err_to_name(evt->error), (int)evt->item_end.reason);
                    xEventGroupSetBits(s_evt, EVT_OTA_FAIL);
                    break;
            }
            break;
        case ESP_OTA_SERVICE_EVT_SESSION_END: {
            const uint16_t ok = evt->session_end.success_count;
            const uint16_t fail = evt->session_end.failed_count;
            const uint16_t skip = evt->session_end.skipped_count;
            if (evt->session_end.aborted || fail > 0) {
                if (ok > 0 && !evt->session_end.aborted) {
                    ESP_LOGW(TAG, "[OTA] Partial success: ok=%u fail=%u skip=%u", ok, fail, skip);
                }
                xEventGroupSetBits(s_evt, EVT_OTA_DONE | EVT_OTA_FAIL);
            } else if (ok > 0) {
                /* At least one item upgraded — caller must reboot to apply it. */
                ESP_LOGI(TAG, "[OTA] Session finished: ok=%u skip=%u", ok, skip);
                xEventGroupSetBits(s_evt, EVT_OTA_DONE | EVT_OTA_SUCCESS);
            } else {
                /* Every item was skipped; running image is already up to date. */
                ESP_LOGI(TAG, "[OTA] Session finished: all %u item(s) skipped, nothing to update", skip);
                xEventGroupSetBits(s_evt, EVT_OTA_DONE);
            }
            break;
        }
        case ESP_OTA_SERVICE_EVT_MAX:
            break;
    }
}

static void build_fs_path(char *out, size_t out_sz, const char *mount_point, const char *filename)
{
    snprintf(out, out_sz, "%s/%s", mount_point, filename);
}

static void build_fs_uri(char *out, size_t out_sz, const char *mount_point, const char *filename)
{
    snprintf(out, out_sz, "file://%s/%s", mount_point, filename);
}

esp_err_t esp_ota_service_fs_example_run_ota(const char *mount_point)
{
    if (mount_point == NULL) {
        ESP_LOGE(TAG, "Run_ota failed: mount_point is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char firmware_path[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];
    char firmware_uri[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];
    char data_path[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];
    char data_uri[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];
    char tone_path[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];
    char tone_uri[ESP_OTA_SERVICE_FS_EXAMPLE_PATH_MAX];

    build_fs_path(firmware_path, sizeof(firmware_path), mount_point, FIRMWARE_FILENAME);
    build_fs_uri(firmware_uri, sizeof(firmware_uri), mount_point, FIRMWARE_FILENAME);
    build_fs_path(data_path, sizeof(data_path), mount_point, DATA_FILENAME);
    build_fs_uri(data_uri, sizeof(data_uri), mount_point, DATA_FILENAME);
    build_fs_path(tone_path, sizeof(tone_path), mount_point, TONE_FILENAME);
    build_fs_uri(tone_uri, sizeof(tone_uri), mount_point, TONE_FILENAME);

    struct stat st_fw;
    if (stat(firmware_path, &st_fw) != 0) {
        ESP_LOGI(TAG, "Run_ota: no %s on %s — nothing to upgrade", FIRMWARE_FILENAME, mount_point);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Run_ota: found %s (%ld bytes)", firmware_path, (long)st_fw.st_size);

    /* Each data-partition upgrade is independent: add an item only if the
     * corresponding file is present on the mounted volume. */
    struct stat st_data;
    struct stat st_tone;
    bool have_userdata_bin = (stat(data_path, &st_data) == 0);
    bool have_tone_bin = (stat(tone_path, &st_tone) == 0);

    if (have_userdata_bin) {
        ESP_LOGI(TAG, "Run_ota: found %s (%ld bytes) → partition \"%s\" (4-byte semver checker)",
                 data_path, (long)st_data.st_size, USERDATA_LABEL);
    }
    if (have_tone_bin) {
        ESP_LOGI(TAG, "Run_ota: found %s (%ld bytes) → partition \"%s\" (tone-desc checker)",
                 tone_path, (long)st_tone.st_size, FLASH_TONE_LABEL);
    }

    s_evt = xEventGroupCreate();
    if (!s_evt) {
        ESP_LOGE(TAG, "Run_ota failed: event group create returned NULL (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_FS_EX_ERR_NO_EVENT_GROUP));
        return ESP_OTA_SERVICE_FS_EX_ERR_NO_EVENT_GROUP;
    }

    esp_ota_service_cfg_t svc_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();
    esp_ota_service_t *svc = NULL;
    esp_err_t ret = esp_ota_service_create(&svc_cfg, &svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_ota_service_create returned %s", esp_err_to_name(ret));
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        return ret;
    }

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.handler = on_ota_adf_event;
    ret = esp_service_event_subscribe((esp_service_t *)svc, &sub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_service_event_subscribe returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        return ret;
    }

    esp_ota_service_source_t *src_app = NULL;
    esp_ota_service_target_t *tgt_app = NULL;
    esp_ota_service_checker_t *chk_app = NULL;
    esp_ota_service_source_t *src_data = NULL;
    esp_ota_service_target_t *tgt_data = NULL;
    esp_ota_service_checker_t *chk_data = NULL;
    esp_ota_service_source_t *src_tone = NULL;
    esp_ota_service_target_t *tgt_tone = NULL;
    esp_ota_service_checker_t *chk_tone = NULL;

    esp_err_t ocr = esp_ota_service_source_fs_create(NULL, &src_app);
    if (ocr == ESP_OK) {
        esp_ota_service_target_app_cfg_t app_cfg = {.bulk_flash_erase = true};
        ocr = esp_ota_service_target_app_create(&app_cfg, &tgt_app);
    }
    if (ocr == ESP_OK) {
        chk_app = esp_ota_service_checker_app_create(NULL);
        if (!chk_app) {
            ocr = ESP_ERR_NO_MEM;
        }
    }

    const esp_app_desc_t *app = esp_app_get_description();

    if (ocr == ESP_OK && have_userdata_bin) {
        ocr = esp_ota_service_source_fs_create(NULL, &src_data);
        if (ocr == ESP_OK) {
            ocr = esp_ota_service_target_data_create(NULL, &tgt_data);
        }
        if (ocr == ESP_OK) {
            esp_ota_service_checker_data_cfg_t dcfg = {.current_version = app->version};
            chk_data = esp_ota_service_checker_data_version_create(&dcfg);
            if (!chk_data) {
                ocr = ESP_ERR_NO_MEM;
            }
        }
    }

    if (ocr == ESP_OK && have_tone_bin) {
        ocr = esp_ota_service_source_fs_create(NULL, &src_tone);
        if (ocr == ESP_OK) {
            ocr = esp_ota_service_target_data_create(NULL, &tgt_tone);
        }
        if (ocr == ESP_OK) {
            /* Tone bin: read the esp_app_desc_t embedded at offset 8 (tone header size).
             * Default magic 0xF55F9876 matches mk_audio_tone.py format=1. project_name
             * check is disabled because the running app's project_name won't match
             * "ESP_TONE_BIN"; we only require a strictly-higher semver.
             *
             * Current tone version is read from the installed partition, NOT from
             * app->version.  Using app->version caused an infinite reboot loop:
             * the tone version (e.g. 0.5.0) is always higher than the app version
             * (e.g. 0.3.0), so the checker always reported an upgrade available,
             * even after the tone partition had just been written. */
            char tone_cur_ver[32] = "0.0.0";
            if (read_tone_partition_version(tone_cur_ver, sizeof(tone_cur_ver))) {
                ESP_LOGI(TAG, "Run_ota: installed tone version: %s", tone_cur_ver);
            } else {
                ESP_LOGW(TAG, "Run_ota: tone partition unreadable, assuming '0.0.0'");
            }
            esp_ota_service_checker_tone_desc_cfg_t tcfg = {
                .current_version = tone_cur_ver,
                .policy = {
                    .require_higher_version = true,
                    .check_project_name = false,
                },
            };
            chk_tone = esp_ota_service_checker_tone_desc_create(&tcfg);
            if (!chk_tone) {
                ocr = ESP_ERR_NO_MEM;
            }
        }
    }

    if (ocr != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: OTA component create returned %s", esp_err_to_name(ocr));
        if (src_app && src_app->destroy) {
            src_app->destroy(src_app);
        }
        if (tgt_app && tgt_app->destroy) {
            tgt_app->destroy(tgt_app);
        }
        if (chk_app && chk_app->destroy) {
            chk_app->destroy(chk_app);
        }
        if (src_data && src_data->destroy) {
            src_data->destroy(src_data);
        }
        if (tgt_data && tgt_data->destroy) {
            tgt_data->destroy(tgt_data);
        }
        if (chk_data && chk_data->destroy) {
            chk_data->destroy(chk_data);
        }
        if (src_tone && src_tone->destroy) {
            src_tone->destroy(src_tone);
        }
        if (tgt_tone && tgt_tone->destroy) {
            tgt_tone->destroy(tgt_tone);
        }
        if (chk_tone && chk_tone->destroy) {
            chk_tone->destroy(chk_tone);
        }
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        return ocr;
    }

    /* Order matters: app first, then data partitions so their writes land
     * before the application reboots the device after SESSION_END. */
    esp_ota_upgrade_item_t items[3];
    int nitems = 0;

    items[nitems++] = (esp_ota_upgrade_item_t) {
        .uri = firmware_uri,
        .partition_label = NULL,
        .source = src_app,
        .target = tgt_app,
        .verifier = NULL,
        .checker = chk_app,
        .skip_on_fail = false,
        .resumable = true,
    };

    if (have_userdata_bin) {
        items[nitems++] = (esp_ota_upgrade_item_t) {
            .uri = data_uri,
            .partition_label = USERDATA_LABEL,
            .source = src_data,
            .target = tgt_data,
            .verifier = NULL,
            .checker = chk_data,
            .skip_on_fail = true,
            .resumable = false,
        };
    }

    if (have_tone_bin) {
        items[nitems++] = (esp_ota_upgrade_item_t) {
            .uri = tone_uri,
            .partition_label = FLASH_TONE_LABEL,
            .source = src_tone,
            .target = tgt_tone,
            .verifier = NULL,
            .checker = chk_tone,
            .skip_on_fail = true,
            .resumable = false,
        };
    }

    ret = esp_ota_service_set_upgrade_list(svc, items, nitems);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_ota_service_set_upgrade_list returned %s", esp_err_to_name(ret));
        /* Ownership of every source/target/checker transferred to the service
         * the moment esp_ota_service_set_upgrade_list() was entered; the service
         * already destroyed them on this error path. Do NOT destroy them here
         * — doing so causes a double-free / heap corruption. */
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        return ret;
    }

    /* Ownership of source/target/checker/verifier objects has transferred
     * to the service via set_upgrade_list.  esp_ota_service_destroy() will
     * free them — do NOT destroy them manually from here on. */

    ESP_LOGI(TAG, "Run_ota: starting OTA (%d item(s)) ...", nitems);
    ret = esp_service_start((esp_service_t *)svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_service_start returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        return ret;
    }

    /* Wait for the OTA session to fully complete (EVT_OTA_DONE is set only
     * at SESSION_END, guaranteeing the worker has finished before we clean up).
     * EVT_OTA_SKIP is intentionally excluded: it is set per-item and fires
     * before the session ends, which would cause premature cleanup while the
     * OTA worker is still running and its callbacks still reference s_evt. */
    const TickType_t session_deadline = pdMS_TO_TICKS(120000);
    EventBits_t bits = xEventGroupWaitBits(s_evt, EVT_OTA_DONE | EVT_OTA_FAIL,
                                           pdFALSE, pdFALSE, session_deadline);

    esp_err_t out = ESP_OK;

    if ((bits & EVT_OTA_DONE) && (bits & EVT_OTA_SUCCESS) && !(bits & EVT_OTA_FAIL)) {
        /* At least one item upgraded — reboot to apply the new image. */
        ESP_LOGI(TAG, "Run_ota: OTA session finished successfully — rebooting to apply new firmware ...");
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_evt);
        s_evt = NULL;
        esp_restart();
    } else if ((bits & EVT_OTA_DONE) && !(bits & EVT_OTA_FAIL)) {
        /* Session complete but every item was skipped; nothing changed. */
        ESP_LOGI(TAG, "Run_ota: all items up to date, no firmware applied");
        out = ESP_OK;
    } else if (bits & EVT_OTA_FAIL) {
        ESP_LOGE(TAG, "Run_ota failed: OTA session failed (%s)", esp_err_to_name(ESP_OTA_SERVICE_FS_EX_ERR_OTA_GENERIC));
        out = ESP_OTA_SERVICE_FS_EX_ERR_OTA_GENERIC;
    } else {
        ESP_LOGE(TAG, "Run_ota failed: wait for OTA completion timed out (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_FS_EX_ERR_OTA_WAIT_TIMEOUT));
        esp_service_stop((esp_service_t *)svc);
        out = ESP_OTA_SERVICE_FS_EX_ERR_OTA_WAIT_TIMEOUT;
    }

    esp_ota_service_destroy(svc);
    vEventGroupDelete(s_evt);
    s_evt = NULL;
    return out;
}
