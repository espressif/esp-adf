/* WiFi Connection Example using WPA2 Enterprise

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_mem.h"
#include "periph_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "wpa2_enterprise";

extern uint8_t ca_pem_start[]     asm("_binary_wpa2_ca_pem_start");
extern uint8_t ca_pem_end[]       asm("_binary_wpa2_ca_pem_end");
extern uint8_t client_crt_start[] asm("_binary_wpa2_client_crt_start");
extern uint8_t client_crt_end[]   asm("_binary_wpa2_client_crt_end");
extern uint8_t client_key_start[] asm("_binary_wpa2_client_key_start");
extern uint8_t client_key_end[]   asm("_binary_wpa2_client_key_end");

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 0 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 1 ] Set Wi-Fi config");
    periph_wifi_cfg_t wifi_cfg = {.wifi_config.sta.ssid = CONFIG_WIFI_SSID};
    wifi_cfg.wpa2_e_cfg.diasble_wpa2_e = true;
    wifi_cfg.wpa2_e_cfg.eap_method = CONFIG_EAP_METHOD;
    wifi_cfg.wpa2_e_cfg.ca_pem_start = (char *)ca_pem_start;
    wifi_cfg.wpa2_e_cfg.ca_pem_start = (char *)ca_pem_end;
    wifi_cfg.wpa2_e_cfg.wpa2_e_cert_start = (char *)client_crt_start;
    wifi_cfg.wpa2_e_cfg.wpa2_e_cert_end = (char *)client_crt_end;
    wifi_cfg.wpa2_e_cfg.wpa2_e_key_start = (char *)client_key_start;
    wifi_cfg.wpa2_e_cfg.wpa2_e_key_end = (char *)client_key_end;
    wifi_cfg.wpa2_e_cfg.eap_id = CONFIG_EAP_ID;
    wifi_cfg.wpa2_e_cfg.eap_username = CONFIG_EAP_USERNAME;
    wifi_cfg.wpa2_e_cfg.eap_password = CONFIG_EAP_PASSWORD;

    ESP_LOGI(TAG, "[ 2 ] Start bt peripheral");
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);

    ESP_LOGI(TAG, "[ 3 ] Get ip info");
    while (1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        esp_netif_ip_info_t ip_info;
        int ret = esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
#else
        tcpip_adapter_ip_info_t ip_info;
        int ret = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
#endif
        if (ret == 0) {
            ESP_LOGI(TAG, "IP:"IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "MASK:"IPSTR, IP2STR(&ip_info.netmask));
            ESP_LOGI(TAG, "GW:"IPSTR, IP2STR(&ip_info.gw));
        }
    }
    /* Stop all peripherals */
    esp_periph_set_stop_all(set);

     /* Release all resources */
    esp_periph_set_destroy(set);
}
