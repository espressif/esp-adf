/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "esp_gmf_err.h"
#include "av_rec_config.h"

static const char *TAG = "AV_REC_BOARD";

static esp_capture_format_id_t get_display_format(const dev_display_lcd_config_t *lcd_cfg)
{
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
    /* CSI outputs o_uyy_e_vyy; PPA can convert to RGB565 but cannot use byte_swap on YUV input
     * (see ppa_srm: in.srm_cm does not support byte_swap). Always use LE RGB565 for display. */
    (void)lcd_cfg;
    return ESP_CAPTURE_FMT_ID_RGB565;
#else
    if (lcd_cfg && lcd_cfg->data_endian == LCD_RGB_DATA_ENDIAN_BIG) {
        return ESP_CAPTURE_FMT_ID_RGB565_BE;
    }
    return ESP_CAPTURE_FMT_ID_RGB565;
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */
}

static void set_default_display_resolution(esp_capture_video_info_t *display_info, const dev_display_lcd_config_t *lcd_cfg)
{
    display_info->format_id = get_display_format(lcd_cfg);
    display_info->width = RECORD_WIDTH;
    display_info->height = RECORD_HEIGHT;
    display_info->fps = DISPLAY_FPS;
    if (lcd_cfg) {
        display_info->width = lcd_cfg->lcd_width < RECORD_WIDTH ? lcd_cfg->lcd_width : RECORD_WIDTH;
        display_info->height = lcd_cfg->lcd_height < RECORD_HEIGHT ? lcd_cfg->lcd_height : RECORD_HEIGHT;
    }
}

static void update_deinit_result(esp_err_t err, esp_err_t *ret, const char *name)
{
    if (err == ESP_OK) {
        return;
    }
    ESP_LOGW(TAG, "Failed to deinit %s, ret=%d", name, err);
    if (*ret == ESP_OK) {
        *ret = err;
    }
}

esp_err_t av_rec_init_devices(av_record_live_display_sys_t *sys)
{
    esp_err_t ret = ESP_OK;
#if CONFIG_IDF_TARGET_ESP32P4
    ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init MIPI LDO");
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

    ESP_LOGI(TAG, "[ 1 ] Initialize display, storage, camera and audio ADC");
#if CONFIG_IDF_TARGET_ESP32S3
    /* SPI LCD on boards such as ESP32-S3-Korvo2 V3 uses TCA9554 lines for CS/reset; must init before display_lcd. */
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_GPIO_EXPANDER);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init GPIO expander");
#endif  /* CONFIG_IDF_TARGET_ESP32S3 */

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init LCD display");
#if CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD touch not available, ret=%d", ret);
    }
#endif  /* CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT */
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init SD card");
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init camera");
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to init audio ADC");

    dev_audio_codec_handles_t *rec_dev_handle = NULL;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&rec_dev_handle);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to get audio ADC device handle");
    ESP_GMF_CHECK(TAG, rec_dev_handle != NULL && rec_dev_handle->codec_dev != NULL, return ESP_FAIL, "Audio ADC device handle invalid");
    esp_codec_dev_handle_t record_handle = rec_dev_handle->codec_dev;
    ret = esp_codec_dev_set_in_gain(record_handle, 25);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ret, "Failed to set input gain");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&sys->lcd_handles);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ESP_FAIL, "Failed to get LCD panel handle");
    ESP_GMF_CHECK(TAG, sys->lcd_handles != NULL && sys->lcd_handles->panel_handle != NULL, return ESP_FAIL, "LCD panel handle invalid");
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&sys->lcd_cfg);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return ESP_FAIL, "Failed to get LCD config");
    ESP_GMF_CHECK(TAG, sys->lcd_cfg != NULL, return ESP_FAIL, "LCD config is NULL");
#if CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT
    dev_lcd_touch_handles_t *touch_handles = NULL;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, (void **)&touch_handles);
    if (ret == ESP_OK && touch_handles != NULL && touch_handles->touch_handle != NULL) {
        sys->touch_handle = touch_handles->touch_handle;
        ESP_LOGI(TAG, "LCD touch ready");
    } else {
        sys->touch_handle = NULL;
        ESP_LOGW(TAG, "LCD touch handle unavailable, ret=%d", ret);
    }
#endif  /* CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT */

    set_default_display_resolution(&sys->display_info, sys->lcd_cfg);
    ESP_LOGI(TAG, "Display sink format=0x%08" PRIx32 " size=%ux%u fps=%u",
             sys->display_info.format_id, sys->display_info.width, sys->display_info.height, sys->display_info.fps);
    return ESP_OK;
}

esp_err_t av_rec_deinit_devices(void)
{
    esp_err_t ret = ESP_OK;
    esp_err_t err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_CAMERA);
    err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_FS_SDCARD);
#if CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT
    err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
#endif  /* CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT */
    err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
#if CONFIG_IDF_TARGET_ESP32S3
    err = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_GPIO_EXPANDER);
    update_deinit_result(err, &ret, ESP_BOARD_DEVICE_NAME_GPIO_EXPANDER);
#elif CONFIG_IDF_TARGET_ESP32P4
    err = esp_board_periph_deinit(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    update_deinit_result(err, &ret, ESP_BOARD_PERIPH_NAME_LDO_MIPI);
#endif  /* CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4 */
    return ret;
}
