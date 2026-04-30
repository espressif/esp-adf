idf_component_register(
    SRCS         "main.c"
                 "smoke_test.c"
    INCLUDE_DIRS "."
    REQUIRES     button_service
                 cloud_service
                 led_service
                 ota_service
                 player_service
                 wifi_service
                 esp_service
)
