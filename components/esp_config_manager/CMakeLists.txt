idf_component_register(
    SRCS
        "src/esp_config_manager.c"
        "src/esp_config_storage.c"
        "src/esp_config_storage_nvs.c"
        "src/esp_config_storage_fs.c"
        "src/esp_config_storage_flash.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer nvs_flash esp_partition heap esp_rom
)
