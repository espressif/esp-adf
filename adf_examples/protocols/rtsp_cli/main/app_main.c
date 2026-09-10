/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_video_dec_default.h"
#include "esp_video_enc_default.h"
#include "media_lib_adapter.h"

#include "esp_config_storage.h"
#include "esp_service.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"

#include "rtsp_cli.h"
#include "rtsp_example.h"
#include "rtsp_settings.h"

#define RTSP_LCD_FRAME_BUFFER_NUM  (2)

/* esp_wifi_service performs one direct attempt per request and does not retry.
   Retry so ESP32-P4 can recover when the first attempt starts before the
   esp_hosted slave is ready. */
#define RTSP_WIFI_ATTEMPTS          3
#define RTSP_WIFI_ATTEMPT_WAIT_SEC  10

static const char *TAG = "RTSP_EXAMPLE";

static esp_config_storage_t s_wifi_store;
static esp_wifi_service_profile_mgr_t s_wifi_profiles;
static esp_wifi_service_t *s_wifi_service;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Erase NVS");
        ret = nvs_flash_init();
    }
    return ret;
}

#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
/* esp_video_render only draws straight into the panel frame buffers when the panel
   owns more than one. With a single buffer every frame is copied, which tears on
   720p video. The count is fixed at panel creation, so raise it before init. */
static void use_double_frame_buffer(void)
{
    dev_display_lcd_config_t *board_cfg = NULL;
    if (esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&board_cfg) != ESP_OK) {
        return;
    }

    dev_display_lcd_config_t lcd_cfg;
    memcpy(&lcd_cfg, board_cfg, sizeof(lcd_cfg));
#ifdef CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    if (strcmp(board_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        lcd_cfg.sub_cfg.dsi.dpi_config.num_fbs = RTSP_LCD_FRAME_BUFFER_NUM;
    }
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT */
#ifdef CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
    if (strcmp(board_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0) {
        lcd_cfg.sub_cfg.rgb.panel_config.num_fbs = RTSP_LCD_FRAME_BUFFER_NUM;
    }
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT */

    if (esp_board_device_override_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &lcd_cfg, sizeof(lcd_cfg)) != ESP_OK) {
        ESP_LOGW(TAG, "Keeping the board frame buffer count");
    }
}
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */

static void apply_mic_gain(void)
{
    dev_audio_codec_handles_t *adc = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc) != ESP_OK ||
        adc == NULL || adc->codec_dev == NULL) {
        return;
    }
    if (esp_codec_dev_set_in_gain(adc->codec_dev, RTSP_MIC_GAIN) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to set mic gain to %.1f dB", RTSP_MIC_GAIN);
    }
}

/* Every device is optional: a board without a camera can still pull and play,
   and a board without a display can still capture and send. */
static void init_board(void)
{
#if CONFIG_IDF_TARGET_ESP32P4
    esp_err_t ldo_ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    if (ldo_ret != ESP_OK) {
        ESP_LOGW(TAG, "LDO MIPI init skipped: %s", esp_err_to_name(ldo_ret));
    }
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    use_double_frame_buffer();
    esp_err_t lcd_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (lcd_ret != ESP_OK) {
        ESP_LOGW(TAG, "Display unavailable (%s); the pull role plays audio only",
                 esp_err_to_name(lcd_ret));
    }
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */

#ifdef ESP_BOARD_DEVICE_NAME_CAMERA
    esp_err_t cam_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (cam_ret != ESP_OK) {
        ESP_LOGW(TAG, "Camera unavailable (%s); server and push roles run audio only",
                 esp_err_to_name(cam_ret));
    }
#endif  /* ESP_BOARD_DEVICE_NAME_CAMERA */

    esp_err_t adc_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    if (adc_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio ADC unavailable: %s", esp_err_to_name(adc_ret));
    } else {
        apply_mic_gain();
    }

    esp_err_t dac_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    if (dac_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio DAC unavailable: %s", esp_err_to_name(dac_ret));
    }
}

/* Encoders feed the send roles, decoders feed the pull role */
static esp_err_t register_codecs(void)
{
    ESP_RETURN_ON_ERROR(esp_audio_enc_register_default(), TAG, "Register audio encoders");
    ESP_RETURN_ON_ERROR(esp_video_enc_register_default(), TAG, "Register video encoders");
    ESP_RETURN_ON_ERROR(esp_audio_dec_register_default(), TAG, "Register audio decoders");
    ESP_RETURN_ON_ERROR(esp_video_dec_register_default(), TAG, "Register video decoders");
    return ESP_OK;
}

static esp_err_t init_wifi(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Init netif");

    static esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "rtsp_wifi",
        .key_primary = "prof_p",
        .key_backup = "prof_b",
    };
    ESP_RETURN_ON_ERROR(esp_config_storage_init_nvs(&nvs_cfg, &s_wifi_store), TAG, "Init Wi-Fi store");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = 4,
        .storage = s_wifi_store,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_profile_mgr_init(&profile_cfg, &s_wifi_profiles), TAG, "Init profiles");

    esp_wifi_service_config_t wifi_cfg = {
        .name = "rtsp_wifi",
        .profile_manager = s_wifi_profiles,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_create(&wifi_cfg, &s_wifi_service), TAG, "Create Wi-Fi service");
    return esp_service_start(ESP_SERVICE_BASE(s_wifi_service));
}

esp_err_t rtsp_example_wifi_connect(const char *ssid, const char *password)
{
    if (s_wifi_service == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char ssid_buf[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1];
    char pass_buf[ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN + 1];
    snprintf(ssid_buf, sizeof(ssid_buf), "%s", ssid != NULL ? ssid : "");
    snprintf(pass_buf, sizeof(pass_buf), "%s", password != NULL ? password : "");
    if (ssid_buf[0] == '\0') {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty; set it in menuconfig or run 'wifi <ssid> <password>'");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= RTSP_WIFI_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID:%s (attempt %d/%d)", ssid_buf, attempt, RTSP_WIFI_ATTEMPTS);
        ret = esp_wifi_service_request_connect(s_wifi_service, ssid_buf, pass_buf, 10, RTSP_WIFI_ATTEMPT_WAIT_SEC);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi connected");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Wi-Fi attempt %d/%d failed: %s", attempt, RTSP_WIFI_ATTEMPTS, esp_err_to_name(ret));
    }

    ESP_LOGW(TAG, "Wi-Fi connect failed (%s); RTSP sessions will not reach the network. "
                  "Reconnect at any time with 'wifi <ssid> <password>'", esp_err_to_name(ret));
    return ret;
}

void rtsp_example_fill_ip(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return;
    }
    snprintf(buf, buf_len, "DEVICE_IP");

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return;
    }
    esp_netif_ip_info_t ip = {0};
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        snprintf(buf, buf_len, IPSTR, IP2STR(&ip.ip));
    }
}

void app_main(void)
{
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);

    ESP_LOGI(TAG, "=== RTSP Example ===");

    ESP_LOGI(TAG, "[ 1 ] Initialize NVS and the media adapter");
    ESP_ERROR_CHECK(init_nvs());
    media_lib_add_default_adapter();

    ESP_LOGI(TAG, "[ 2 ] Initialize board devices (camera / LCD / audio)");
    init_board();

    ESP_LOGI(TAG, "[ 3 ] Register audio and video codecs");
    ESP_ERROR_CHECK(register_codecs());

    ESP_LOGI(TAG, "[ 4 ] Connect to WiFi");
    ESP_ERROR_CHECK(init_wifi());
    (void)rtsp_example_wifi_connect(RTSP_WIFI_SSID, RTSP_WIFI_PASSWORD);

    ESP_LOGI(TAG, "[ 5 ] Start CLI");
    ESP_ERROR_CHECK(rtsp_cli_start());

    ESP_LOGI(TAG, "CLI ready");
    ESP_LOGI(TAG, "Type 'help' to list all commands");
}
