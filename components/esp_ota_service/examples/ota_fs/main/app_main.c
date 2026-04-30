/**
 * OTA File-System Upgrade Example — ESP32-S3-Korvo-2 V3 / ESP32-P4
 *
 * Two storage backends, selected at build time via Kconfig
 * ("OTA FS Example → Storage backend for OTA bin files"):
 *
 *   - CONFIG_OTA_FS_EXAMPLE_STORAGE_SDCARD (default):
 *         SD card via ESP Board Manager — single device only
 *         (no esp_board_manager_init):
 *           - Preferred: esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD)
 *           - Equivalent: esp_board_device_init(ESP_BOARD_DEVICE_NAME_FS_SDCARD)
 *         Both resolve to the same init path; they do NOT run
 *         esp_board_device_init_all(). Requires components/gen_bmgr_codes
 *         for this board (idf.py gen-bmgr-config -b esp32_s3_korvo2_v3).
 *         Dependency: main/idf_component.yml → espressif/esp_board_manager.
 *
 *   - CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC:
 *         USB Host + Mass Storage Class driver (managed
 *         espressif/usb_host_msc). Waits for a USB flash drive to be
 *         inserted, mounts its FAT volume at CONFIG_OTA_FS_EXAMPLE_USB_MOUNT_POINT
 *         (default "/usb"), then runs OTA from
 *         file://<mount>/firmware.bin via esp_ota_service_source_fs_create().
 *         Supported on ESP32-S2 / ESP32-S3 / ESP32-P4 (USB-OTG host).
 *
 * A self-contained firmware that:
 *   1. Boots, prints its own version (from esp_app_desc_t / version.txt).
 *   2. Initializes the selected storage backend:
 *         - SDCARD:  fs_sdcard board device (FAT + VFS mount at /sdcard per board YAML).
 *         - USB_MSC: USB Host + MSC driver, mounts the first flash drive at /usb.
 *   3. If <mount>/firmware.bin exists: OTA to the next app slot (version must
 *      be newer when using the version verifier).
 *   4. If data images are present, flashes each into its dedicated data
 *      partition (see partitions.csv). Both items are independent — any subset
 *      of the two may be present:
 *        a) /sdcard/userdata.bin   → partition "userdata"  (0x32000 bytes)
 *           4-byte little-endian packed-semver header
 *           (esp_ota_service_version_pack_semver("x.y.z"), major<<16|minor<<8|patch).
 *           Checker: esp_ota_service_checker_data_version_create(). Generated automatically by
 *           tools/build_firmware.py fs (emits userdata_v<ver>.bin).
 *        b) /sdcard/flash_tone.bin → partition "flash_tone" (0x32000 bytes)
 *           Upstream audio_tone format=1 layout with esp_app_desc_t embedded at
 *           offset 8 (magic 0xF55F9876, project_name "ESP_TONE_BIN").
 *           Checker: esp_ota_service_checker_tone_desc_create(). Generator (upstream esp-adf):
 *               python tools/audio_tone/mk_audio_tone.py -f out -r resources \
 *                   -v 1.0.0 -F 1 -t flash_tone.bin
 *      All items run in one session; the application reboots once when the
 *      session succeeds (the OTA service itself never calls esp_restart()).
 *   5. On idle, logs the decoded userdata version header and a hex preview so you can
 *      confirm the data partition after upgrade.
 *
 * Usage (steps 2-3 run from components/esp_ota_service/tools/):
 *   1. Set version in version.txt, idf.py build && idf.py flash
 *   2. Bump version.txt, build + publish a newer ota_fs payload (with a
 *      matching userdata bin):
 *          ./build_firmware.py fs -v 0.2.0
 *   3. Stage the payload onto the SD card (also builds sdcard_writer):
 *          ./ota_fs_flash_sdcard.py --port /dev/ttyUSB0
 *   4. Reset the board to run OTA. After OTA, logs decode the userdata header
 *      and compare to the running app version.
 *   5. Breakpoint resume (NVS): end-to-end automation is in
 *      tools/ota_fs_test_resume.py. The script builds + stages a newer bin,
 *      reflashes the older version, triggers a hard-reset mid-OTA once
 *      progress passes the threshold, and asserts that OTA_SERVICE logs
 *      "Resuming from offset ..." on reboot.
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_ota_service_fs_example.h"

static const char *TAG = "OTA_FS_APP";

#if defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
#define ESP_OTA_SERVICE_FS_EXAMPLE_MOUNT_POINT  CONFIG_OTA_FS_EXAMPLE_USB_MOUNT_POINT
#else
#define ESP_OTA_SERVICE_FS_EXAMPLE_MOUNT_POINT  "/sdcard"
#endif  /* defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */

static esp_err_t esp_ota_service_fs_example_init_storage(void)
{
#if defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
    return esp_ota_service_fs_example_init_usb();
#else
    return esp_ota_service_fs_example_init_sdcard();
#endif  /* defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */
}

static esp_err_t esp_ota_service_fs_example_deinit_storage(void)
{
#if defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
    return esp_ota_service_fs_example_deinit_usb();
#else
    return esp_ota_service_fs_example_deinit_sdcard();
#endif  /* defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Project:  %s", app->project_name);
    ESP_LOGI(TAG, "  Version:  %s", app->version);
    ESP_LOGI(TAG, "  Built:    %s %s", app->date, app->time);

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "  Partition: %s (0x%08" PRIx32 ")",
             running ? running->label : "<unknown>",
             running ? running->address : 0);
#if defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
    ESP_LOGI(TAG, "  Storage:   USB Host MSC (mount %s)", ESP_OTA_SERVICE_FS_EXAMPLE_MOUNT_POINT);
#else
    ESP_LOGI(TAG, "  Storage:   SD card (mount %s)", ESP_OTA_SERVICE_FS_EXAMPLE_MOUNT_POINT);
#endif  /* defined(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC) */
    ESP_LOGI(TAG, "========================================");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = esp_ota_service_fs_example_init_storage();
    if (err == ESP_OK) {
        (void)esp_ota_service_fs_example_run_ota(ESP_OTA_SERVICE_FS_EXAMPLE_MOUNT_POINT);
        (void)esp_ota_service_fs_example_deinit_storage();
    } else {
        /* Init failure already logged by the backend-specific init function. */
    }

    (void)esp_ota_service_fs_example_log_userdata_preview();

    ESP_LOGI(TAG, "Running version %s — idle on %s", app->version,
             running ? running->label : "<unknown>");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "[%s] Still running on %s", app->version,
                 running ? running->label : "<unknown>");
    }
}
