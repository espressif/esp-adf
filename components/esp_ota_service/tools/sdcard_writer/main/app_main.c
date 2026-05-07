/**
 * SD Card Writer Utility
 *
 * Writes embedded firmware payloads to the SD card (names on-card are fixed,
 * versioning lives in the source file chosen by CMake; on-card names match
 * what examples/ota_fs reads in ota_fs_example.c):
 *   - firmware.bin    (always; source: tools/firmware_samples/ota_fs_v*.bin)
 *   - userdata.bin    (if userdata_v*.bin is published)
 *   - flash_tone.bin  (if flash_tone_v*.bin is published)
 *
 * After each write it reads back the first 16 bytes for a sanity check.
 *
 * SD card is brought up through ESP Board Manager (single device init),
 * mirroring the ota_fs example and GMF examples.
 *
 * Requires components/gen_bmgr_codes for this board. Generate it once with:
 *   idf.py gen-bmgr-config -b esp32_s3_korvo2_v3
 *
 * Rebuild / reflash with the latest firmware_samples payload via:
 *   tools/ota_fs_flash_sdcard.py
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "dev_fs_fat.h"
#include "esp_board_device.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"

static const char *TAG = "SD_WRITER";

extern const uint8_t app_bin_start[] asm("_binary_app_bin_start");
extern const uint8_t app_bin_end[] asm("_binary_app_bin_end");

#if SDW_HAS_USERDATA
extern const uint8_t userdata_bin_start[] asm("_binary_userdata_bin_start");
extern const uint8_t userdata_bin_end[] asm("_binary_userdata_bin_end");
#endif  /* SDW_HAS_USERDATA */

#if SDW_HAS_FLASH_TONE
extern const uint8_t flash_tone_bin_start[] asm("_binary_flash_tone_bin_start");
extern const uint8_t flash_tone_bin_end[] asm("_binary_flash_tone_bin_end");
#endif  /* SDW_HAS_FLASH_TONE */

typedef struct {
    const char    *sd_name;
    const uint8_t *data;
    size_t         size;
} payload_t;

static void dump_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

static bool write_payload(const char *mount_point, const payload_t *p)
{
    char target[96];
    snprintf(target, sizeof(target), "%s/%s", mount_point, p->sd_name);

    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, "Writing %zu bytes -> %s", p->size, target);
    dump_hex("  embedded head", p->data, 16);

    if (remove(target) != 0 && errno != ENOENT) {
        ESP_LOGW(TAG, "  remove(%s) failed: %s", target, strerror(errno));
    }

    int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        ESP_LOGE(TAG, "  open(write) failed for %s: %s", target, strerror(errno));
        return false;
    }

#define WRITE_CHUNK  4096
    uint8_t *buf = malloc(WRITE_CHUNK);
    if (!buf) {
        ESP_LOGE(TAG, "  malloc failed");
        close(fd);
        return false;
    }

    size_t written = 0;
    const uint8_t *src = p->data;
    size_t remaining = p->size;
    while (remaining > 0) {
        size_t to_copy = remaining < WRITE_CHUNK ? remaining : WRITE_CHUNK;
        memcpy(buf, src, to_copy);
        ssize_t w = write(fd, buf, to_copy);
        if (w < 0) {
            ESP_LOGE(TAG, "  write failed at offset %zu: %s", written, strerror(errno));
            break;
        }
        if ((size_t)w != to_copy) {
            ESP_LOGE(TAG, "  write short: %zd / %zu at offset %zu", w, to_copy, written);
            break;
        }
        written += (size_t)w;
        src += to_copy;
        remaining -= to_copy;
    }
    free(buf);

    if (written != p->size) {
        ESP_LOGE(TAG, "  write incomplete: %zu / %zu", written, p->size);
        close(fd);
        return false;
    }

    if (fsync(fd) != 0) {
        ESP_LOGW(TAG, "  fsync failed: %s (continuing verify)", strerror(errno));
    }
    if (close(fd) != 0) {
        ESP_LOGE(TAG, "  close(write) failed: %s", strerror(errno));
        return false;
    }

    struct stat st_after_close = {0};
    if (stat(target, &st_after_close) != 0) {
        ESP_LOGE(TAG, "  stat(after close) failed for %s: %s", target, strerror(errno));
        DIR *dir = opendir(mount_point);
        if (!dir) {
            ESP_LOGE(TAG, "  opendir(%s) failed: %s", mount_point, strerror(errno));
        } else {
            ESP_LOGI(TAG, "  listing %s:", mount_point);
            struct dirent *e = NULL;
            while ((e = readdir(dir)) != NULL) {
                ESP_LOGI(TAG, "    - %s", e->d_name);
            }
            closedir(dir);
        }
    } else {
        ESP_LOGI(TAG, "  stat(after close): %ld bytes", (long)st_after_close.st_size);
    }

    fd = open(target, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "  open(readback) failed for %s: %s", target, strerror(errno));
        return false;
    }

    struct stat st = {0};
    if (fstat(fd, &st) != 0) {
        ESP_LOGE(TAG, "  fstat failed: %s", strerror(errno));
        close(fd);
        return false;
    }

    uint8_t rb[16] = {0};
    ssize_t n = read(fd, rb, sizeof(rb));
    if (n < 0) {
        ESP_LOGE(TAG, "  read(readback) failed: %s", strerror(errno));
        close(fd);
        return false;
    }
    close(fd);

    dump_hex("  readback head", rb, (size_t)n);
    bool ok = (n == 16 && memcmp(rb, p->data, 16) == 0 && st.st_size == (long)p->size);
    ESP_LOGI(TAG, "  size on SD: %ld bytes  match=%s", (long)st.st_size, ok ? "YES" : "NO");
    return ok;
}

static esp_err_t sd_writer_init_sdcard(const char **mount_point_out)
{
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init sdcard failed: esp_board_manager_init_device_by_name returned %s", esp_err_to_name(ret));
        return ret;
    }

    dev_fs_fat_handle_t *sd = NULL;
    ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&sd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init sdcard failed: esp_board_device_get_handle returned %s", esp_err_to_name(ret));
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ret;
    }
    if (sd == NULL || sd->mount_point == NULL) {
        ESP_LOGE(TAG, "Init sdcard failed: fs_sdcard handle invalid (NULL handle or mount_point)");
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Fs_sdcard mounted at %s", sd->mount_point);
    if (mount_point_out) {
        *mount_point_out = sd->mount_point;
    }
    return ESP_OK;
}

static void sd_writer_deinit_sdcard(void)
{
    esp_err_t ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Deinit sdcard: esp_board_manager_deinit_device_by_name returned %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SD Card Writer");
    ESP_LOGI(TAG, "========================================");

    const char *mount_point = NULL;
    esp_err_t ret = sd_writer_init_sdcard(&mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card init failed: %s", esp_err_to_name(ret));
        goto done;
    }

    payload_t payloads[3];
    int count = 0;

    payloads[count++] = (payload_t) {
        .sd_name = "firmware.bin",
        .data = app_bin_start,
        .size = (size_t)(app_bin_end - app_bin_start),
    };

#if SDW_HAS_USERDATA
    payloads[count++] = (payload_t) {
        .sd_name = "userdata.bin",
        .data = userdata_bin_start,
        .size = (size_t)(userdata_bin_end - userdata_bin_start),
    };
#endif  /* SDW_HAS_USERDATA */

#if SDW_HAS_FLASH_TONE
    payloads[count++] = (payload_t) {
        .sd_name = "flash_tone.bin",
        .data = flash_tone_bin_start,
        .size = (size_t)(flash_tone_bin_end - flash_tone_bin_start),
    };
#endif  /* SDW_HAS_FLASH_TONE */

    int ok_count = 0;
    for (int i = 0; i < count; i++) {
        if (write_payload(mount_point, &payloads[i])) {
            ok_count++;
        }
    }

    ESP_LOGI(TAG, "----------------------------------------");
    if (ok_count == count) {
        ESP_LOGI(TAG, "*** SUCCESS: %d/%d payload(s) verified on SD card ***", ok_count, count);
    } else {
        ESP_LOGE(TAG, "*** FAILED: %d/%d payload(s) verified ***", ok_count, count);
    }

    sd_writer_deinit_sdcard();
    ESP_LOGI(TAG, "SD card unmounted");

done:
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
