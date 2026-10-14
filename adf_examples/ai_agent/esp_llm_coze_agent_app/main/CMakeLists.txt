idf_component_register(
    SRCS "app_main.c" "app_agent.c"
    INCLUDE_DIRS "."
    REQUIRES
        coze_app_media
        esp_event
        nvs_flash
        spiffs
)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
