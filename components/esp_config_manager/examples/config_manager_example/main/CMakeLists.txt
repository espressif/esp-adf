idf_component_register(
    SRCS "config_manager_example.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_config_manager spiffs
)
