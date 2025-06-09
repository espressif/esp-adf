/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define SDMMC_CLK_PIN  CONFIG_SDMMC_CLK_PIN
#define SDMMC_CMD_PIN  CONFIG_SDMMC_CMD_PIN
#define SDMMC_D0_PIN   CONFIG_SDMMC_D0_PIN
#define SDMMC_D1_PIN   CONFIG_SDMMC_D1_PIN
#define SDMMC_D2_PIN   CONFIG_SDMMC_D2_PIN
#define SDMMC_D3_PIN   CONFIG_SDMMC_D3_PIN

static const char *TAG     = "SDCARD";
static const char  mount[] = "/sdcard";

static sdmmc_card_t *g_card;

int esp_sdcard_init(void)
{
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 32 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SDMMC_CLK_PIN;
    slot_config.cmd = SDMMC_CMD_PIN;
    slot_config.d0 = SDMMC_D0_PIN;

    // Set bus width to use:
#if (SDMMC_D1_PIN != -1)
    slot_config.width = 4;
    slot_config.d1 = SDMMC_D1_PIN;
    slot_config.d2 = SDMMC_D2_PIN;
    slot_config.d3 = SDMMC_D3_PIN;
#else
    slot_config.width = 1;
#endif  /* (SDMMC_D1_PIN != -1) */

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    ret = esp_vfs_fat_sdmmc_mount(mount, &host, &slot_config, &mount_config, &g_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, g_card);
    return ESP_OK;
}

void esp_sdcard_deinit(void)
{
    // All done, unmount partition and disable SDMMC peripheral
    esp_vfs_fat_sdcard_unmount(mount, g_card);
    ESP_LOGI(TAG, "Card unmounted");
}
