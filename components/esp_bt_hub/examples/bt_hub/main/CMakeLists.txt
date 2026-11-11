idf_component_register(SRCS "main.c" "bt_hub_console.c"
                       INCLUDE_DIRS "."
                       REQUIRES console esp_bt_hub nvs_flash)
