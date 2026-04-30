idf_component_register(
    SRCS         "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_service
        adf_event_hub
        esp_button_service
        esp_cli_service
        esp_ota_service
        wifi_service
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        console
)
