/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "soc/soc_caps.h"

#if SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#endif
#include "driver/sdmmc_defs.h"
#include "driver/gpio.h"

#include "sdcard.h"
#include "board.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_ldo_regulator.h"
#endif
#endif
static const char *TAG = "SDCARD";
static int g_gpio = -1;
static sdmmc_card_t *card = NULL;

static void sdmmc_card_print_info(const sdmmc_card_t *card)
{
    ESP_LOGD(TAG, "Name: %s\n", card->cid.name);
    ESP_LOGD(TAG, "Type: %s\n", (card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGD(TAG, "Speed: %s\n", (card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGD(TAG, "Size: %lluMB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    ESP_LOGD(TAG, "CSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d\n",
             card->csd.csd_ver,
             card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
    ESP_LOGD(TAG, "SCR: sd_spec=%d, bus_width=%d\n", card->scr.sd_spec, card->scr.bus_width);
}

static void enable_mmc_phy_power(void)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#if CONFIG_IDF_TARGET_ESP32P4
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    esp_ldo_channel_handle_t ldo_phy_chan;
    esp_ldo_acquire_channel(&ldo_cfg, &ldo_phy_chan);
#endif
#endif
}

esp_err_t sdcard_mount(const char *base_path, periph_sdcard_mode_t mode)
{
    if (mode >= SD_MODE_MAX) {
        ESP_LOGE(TAG, "PLease select the correct sd mode: 1-line SD mode, 4-line SD mode or SPI mode!, current mode is %d", mode);
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_FAIL;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = get_sdcard_open_file_num_max(),
        .allocation_unit_size = 64 * 1024,
    };

    enable_mmc_phy_power();

    if (mode != SD_MODE_SPI) {
#if SOC_SDMMC_HOST_SUPPORTED
        ESP_LOGI(TAG, "Using %d-line SD mode,  base path=%s", mode, base_path);

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        // host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    /* Note: default sdmmc use slot0, hosted use slot1 */
#if defined CONFIG_IDF_TARGET_ESP32P4
        host.slot = SDMMC_HOST_SLOT_0;
#endif // CONFIG_IDF_TARGET_ESP32P4
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        // slot_config.gpio_cd = g_gpio;
        slot_config.width = mode;
        // Enable internal pullups on enabled pins. The internal pullups
        // are insufficient however, please make sure 10k external pullups are
        // connected on the bus. This is for debug / example purpose only.
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

#if SOC_SDMMC_USE_GPIO_MATRIX
        slot_config.clk = ESP_SD_PIN_CLK;
        slot_config.cmd = ESP_SD_PIN_CMD;
        slot_config.d0 = ESP_SD_PIN_D0;
        slot_config.d1 = ESP_SD_PIN_D1;
        slot_config.d2 = ESP_SD_PIN_D2;
        slot_config.d3 = ESP_SD_PIN_D3;
        slot_config.d4 = ESP_SD_PIN_D4;
        slot_config.d5 = ESP_SD_PIN_D5;
        slot_config.d6 = ESP_SD_PIN_D6;
        slot_config.d7 = ESP_SD_PIN_D7;
        slot_config.cd = ESP_SD_PIN_CD;
        slot_config.wp = ESP_SD_PIN_WP;
#endif
        ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot_config, &mount_config, &card);
#endif
    } else {
        ESP_LOGI(TAG, "Using SPI mode, base path=%s", base_path);
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = ESP_SD_PIN_CMD,
            .miso_io_num = ESP_SD_PIN_D0,
            .sclk_io_num = ESP_SD_PIN_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize bus.");
            return ret;
        }
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = ESP_SD_PIN_D3;
        slot_config.host_id = host.slot;
        ret = esp_vfs_fat_sdspi_mount(base_path, &host, &slot_config, &mount_config, &card);
    }

    switch (ret) {
        case ESP_OK:
            // Card has been initialized, print its properties
            sdmmc_card_print_info(card);
            ESP_LOGI(TAG, "CID name %s!\n", card->cid.name);
            break;

        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(TAG, "File system already mounted");
            break;

        case ESP_FAIL:
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
            break;

        default:
            ESP_LOGE(TAG, "Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", ret);
            break;
    }

    return ret;

}

esp_err_t sdcard_unmount(const char *base_path, periph_sdcard_mode_t mode)
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0))
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(base_path, card);
#else
    esp_err_t ret = esp_vfs_fat_sdmmc_unmount();
#endif
    if (mode == SD_MODE_SPI) {
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        spi_bus_free(host.slot);
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "File system not mounted");
    }
    return ret;
}

bool sdcard_is_exist()
{
    if (g_gpio >= 0) {
        return (gpio_get_level(g_gpio) == 0x00);
    } else {
        return true;
    }
    return false;
}

int IRAM_ATTR sdcard_read_detect_pin(void)
{
    if (g_gpio >= 0) {
        return gpio_get_level(g_gpio);
    } else {
        return -1;
    }
    return 0;
}

esp_err_t sdcard_destroy()
{
    if (g_gpio >= 0) {
        return gpio_isr_handler_remove(g_gpio);
    }
    return ESP_OK;
}

esp_err_t sdcard_init(int card_detect_pin, void (*detect_intr_handler)(void *), void *isr_context)
{
    esp_err_t ret = ESP_OK;
    if (card_detect_pin >= 0) {
        gpio_set_direction(card_detect_pin, GPIO_MODE_INPUT);
        if (detect_intr_handler) {
            gpio_set_intr_type(card_detect_pin, GPIO_INTR_ANYEDGE);
            gpio_isr_handler_add(card_detect_pin, detect_intr_handler, isr_context);
            gpio_intr_enable(card_detect_pin);
        }
        gpio_pullup_en(card_detect_pin);
    }
    g_gpio = card_detect_pin;
    return ret;
}
