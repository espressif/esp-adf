idf_component_register(
    SRCS         "app_main.c"
                 "esp_ota_service_http_example.c"
    INCLUDE_DIRS "."
    REQUIRES     nvs_flash esp_wifi esp_netif esp_event esp_ota_service
    PRIV_REQUIRES app_update esp_app_format
)
