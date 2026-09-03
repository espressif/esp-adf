/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_audio_enc_default.h"
#include "esp_video_enc_default.h"
#include "media_lib_adapter.h"

#include "esp_capture_service.h"
#include "esp_config_storage.h"
#include "esp_media_service.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"

#include "rtsp_push_settings.h"

#define RTSP_PUSH_WIFI_ATTEMPTS          3
#define RTSP_PUSH_WIFI_ATTEMPT_WAIT_SEC  10
#define RTSP_PUSH_RUN_DURATION_MS        (15 * 1000)

static const char *TAG = "RTSP_PUSH_EXAMPLE";

static esp_config_storage_t s_wifi_store;
static esp_wifi_service_profile_mgr_t s_wifi_profiles;
static esp_wifi_service_t *s_wifi_service;
static esp_capture_service_t *s_capture;
static esp_rtsp_service_t *s_rtsp;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Erase NVS");
        ret = nvs_flash_init();
    }
    return ret;
}

static void apply_mic_gain(void)
{
    dev_audio_codec_handles_t *adc = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc) != ESP_OK ||
        adc == NULL || adc->codec_dev == NULL) {
        return;
    }
    if (esp_codec_dev_set_in_gain(adc->codec_dev, RTSP_PUSH_MIC_GAIN) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to set mic gain to %.1f dB", RTSP_PUSH_MIC_GAIN);
    }
}

static esp_err_t init_board(void)
{
#if CONFIG_IDF_TARGET_ESP32P4
    ESP_RETURN_ON_ERROR(esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI),
                        TAG, "Initialize camera LDO");
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA),
                        TAG, "Initialize camera");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC),
                        TAG, "Initialize audio ADC");
    apply_mic_gain();
    return ESP_OK;
}

static esp_err_t connect_wifi(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Initialize network interface");

    static const esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "rtsp_push_wifi",
        .key_primary = "prof_p",
        .key_backup = "prof_b",
    };
    ESP_RETURN_ON_ERROR(esp_config_storage_init_nvs(&nvs_cfg, &s_wifi_store),
                        TAG, "Initialize Wi-Fi profile storage");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = 1,
        .storage = s_wifi_store,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_profile_mgr_init(&profile_cfg, &s_wifi_profiles),
                        TAG, "Initialize Wi-Fi profiles");

    esp_wifi_service_config_t wifi_cfg = {
        .name = "rtsp_push_wifi",
        .profile_manager = s_wifi_profiles,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_create(&wifi_cfg, &s_wifi_service),
                        TAG, "Create Wi-Fi service");
    ESP_RETURN_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(s_wifi_service)),
                        TAG, "Start Wi-Fi service");

    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= RTSP_PUSH_WIFI_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID:%s (attempt %d/%d)",
                 RTSP_PUSH_WIFI_SSID, attempt, RTSP_PUSH_WIFI_ATTEMPTS);
        ret = esp_wifi_service_request_connect(s_wifi_service,
                                               RTSP_PUSH_WIFI_SSID,
                                               RTSP_PUSH_WIFI_PASSWORD,
                                               10, RTSP_PUSH_WIFI_ATTEMPT_WAIT_SEC);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi connected");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Wi-Fi attempt %d/%d failed: %s",
                 attempt, RTSP_PUSH_WIFI_ATTEMPTS, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t register_encoders(void)
{
    ESP_RETURN_ON_ERROR(esp_audio_enc_register_default(), TAG, "Register audio encoders");
    ESP_RETURN_ON_ERROR(esp_video_enc_register_default(), TAG, "Register video encoders");
    return ESP_OK;
}

static esp_err_t create_services(void)
{
    esp_video_capture_service_cfg_t cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    cfg.service_name = "rtsp_push_capture";
    cfg.audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    cfg.max_stream_num = 1;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&cfg, &s_capture),
                        TAG, "Create capture service");

    esp_rtsp_service_cfg_t rtsp_cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SINK);
    esp_err_t ret = esp_rtsp_service_create(&rtsp_cfg, &s_rtsp);
    if (ret != ESP_OK) {
        (void)esp_capture_service_destroy(s_capture);
        s_capture = NULL;
        ESP_RETURN_ON_ERROR(ret, TAG, "Create RTSP service");
    }
    return ESP_OK;
}

static esp_err_t setup_services(void)
{
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = RTSP_PUSH_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = {
                .codec = RTSP_PUSH_AUDIO_CODEC,
                .sample_rate = RTSP_PUSH_AUDIO_SAMPLE_RATE,
                .bits_per_sample = RTSP_PUSH_AUDIO_BITS_PER_SAMPLE,
                .channel = RTSP_PUSH_AUDIO_CHANNEL,
                .bitrate = RTSP_PUSH_AUDIO_BITRATE,
            },
            .video_info = {
                .codec = RTSP_PUSH_VIDEO_CODEC,
                .width = RTSP_PUSH_VIDEO_WIDTH,
                .height = RTSP_PUSH_VIDEO_HEIGHT,
                .fps = RTSP_PUSH_VIDEO_FPS,
                .bitrate = RTSP_PUSH_VIDEO_WIDTH * RTSP_PUSH_VIDEO_HEIGHT * RTSP_PUSH_VIDEO_FPS / RTSP_PUSH_VIDEO_BITRATE_DIVISOR,
            },
        },
    };
    ESP_RETURN_ON_ERROR(esp_video_capture_service_apply_setup(s_capture, &setup),
                        TAG, "Configure capture service");

    /* The RTSP sink uses its create-time defaults, so esp_rtsp_service_setup() is optional. */
    ESP_RETURN_ON_ERROR(esp_rtsp_service_set_url(s_rtsp, RTSP_PUSH_URL),
                        TAG, "Set RTSP push URL");

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    ESP_RETURN_ON_FALSE(netif != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "Get station network interface");
    esp_netif_ip_info_t ip_info = {0};
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(netif, &ip_info),
                        TAG, "Get station IP address");
    char local_ip[16];
    snprintf(local_ip, sizeof(local_ip), IPSTR, IP2STR(&ip_info.ip));
    ESP_RETURN_ON_ERROR(esp_rtsp_service_set_ip(s_rtsp, local_ip),
                        TAG, "Set RTSP local IP");

    ESP_RETURN_ON_ERROR(esp_media_service_link(ESP_SERVICE_BASE(s_capture), ESP_MEDIA_DEFAULT_STREAM,
                                               ESP_SERVICE_BASE(s_rtsp), ESP_MEDIA_DEFAULT_STREAM),
                        TAG, "Link capture to RTSP");
    return ESP_OK;
}

static esp_err_t start_rtsp_push(void)
{
    esp_service_state_t capture_state = ESP_SERVICE_STATE_UNINITIALIZED;
    esp_service_state_t rtsp_state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_ERROR(esp_service_get_state(ESP_SERVICE_BASE(s_capture), &capture_state),
                        TAG, "Get capture service state");
    ESP_RETURN_ON_ERROR(esp_service_get_state(ESP_SERVICE_BASE(s_rtsp), &rtsp_state),
                        TAG, "Get RTSP service state");
    ESP_RETURN_ON_FALSE(capture_state == ESP_SERVICE_STATE_INITIALIZED &&
                            rtsp_state == ESP_SERVICE_STATE_INITIALIZED,
                        ESP_ERR_INVALID_STATE, TAG, "RTSP push services are not stopped");

    esp_err_t ret = esp_service_start(ESP_SERVICE_BASE(s_rtsp));
    if (ret != ESP_OK) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_rtsp));
        ESP_RETURN_ON_ERROR(ret, TAG, "Start RTSP service");
    }
    ret = esp_service_start(ESP_SERVICE_BASE(s_capture));
    if (ret != ESP_OK) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_capture));
        (void)esp_service_stop(ESP_SERVICE_BASE(s_rtsp));
        ESP_RETURN_ON_ERROR(ret, TAG, "Start capture service");
    }

    ESP_LOGI(TAG, "Pushing camera and microphone to %s", RTSP_PUSH_URL);
    return ESP_OK;
}

static esp_err_t stop_rtsp_push(void)
{
    esp_err_t capture_ret = esp_service_stop(ESP_SERVICE_BASE(s_capture));
    esp_err_t rtsp_ret = esp_service_stop(ESP_SERVICE_BASE(s_rtsp));
    ESP_RETURN_ON_ERROR(capture_ret, TAG, "Stop capture service");
    ESP_RETURN_ON_ERROR(rtsp_ret, TAG, "Stop RTSP service");
    ESP_LOGI(TAG, "RTSP push stopped");
    return ESP_OK;
}

static esp_err_t deinit_services(void)
{
    esp_err_t unlink_ret = esp_media_service_unlink(ESP_SERVICE_BASE(s_capture), ESP_MEDIA_DEFAULT_STREAM,
                                                    ESP_SERVICE_BASE(s_rtsp), ESP_MEDIA_DEFAULT_STREAM);
    esp_err_t rtsp_ret = esp_media_service_deinit(ESP_SERVICE_BASE(s_rtsp));
    free(s_rtsp);
    s_rtsp = NULL;

    esp_err_t capture_ret = esp_capture_service_destroy(s_capture);
    s_capture = NULL;
    ESP_RETURN_ON_ERROR(unlink_ret, TAG, "Unlink capture from RTSP");
    ESP_RETURN_ON_ERROR(rtsp_ret, TAG, "Deinitialize RTSP service");
    ESP_RETURN_ON_ERROR(capture_ret, TAG, "Destroy capture service");
    ESP_LOGI(TAG, "RTSP push services deinitialized");
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== RTSP Push Example ===");

    ESP_LOGI(TAG, "[ 1 ] Initialize NVS and the media adapter");
    ESP_ERROR_CHECK(init_nvs());
    media_lib_add_default_adapter();

    ESP_LOGI(TAG, "[ 2 ] Initialize the camera and microphone");
    ESP_ERROR_CHECK(init_board());

    ESP_LOGI(TAG, "[ 3 ] Register audio and video encoders");
    ESP_ERROR_CHECK(register_encoders());

    ESP_LOGI(TAG, "[ 4 ] Connect to Wi-Fi");
    ESP_ERROR_CHECK(connect_wifi());

    ESP_LOGI(TAG, "[ 5 ] Create RTSP push services");
    ESP_ERROR_CHECK(create_services());

    ESP_LOGI(TAG, "[ 6 ] Configure and link RTSP push services");
    ESP_ERROR_CHECK(setup_services());

    ESP_LOGI(TAG, "[ 7 ] Start RTSP push");
    ESP_ERROR_CHECK(start_rtsp_push());
    vTaskDelay(pdMS_TO_TICKS(RTSP_PUSH_RUN_DURATION_MS));

    ESP_LOGI(TAG, "[ 8 ] Stop RTSP push");
    ESP_ERROR_CHECK(stop_rtsp_push());

    ESP_LOGI(TAG, "[ 9 ] Restart the same RTSP push services");
    ESP_ERROR_CHECK(start_rtsp_push());
    vTaskDelay(pdMS_TO_TICKS(RTSP_PUSH_RUN_DURATION_MS));

    ESP_LOGI(TAG, "[ 10 ] Stop RTSP push");
    ESP_ERROR_CHECK(stop_rtsp_push());

    ESP_LOGI(TAG, "[ 11 ] Deinitialize RTSP push services");
    ESP_ERROR_CHECK(deinit_services());
    ESP_LOGI(TAG, "RTSP push lifecycle completed");
}
