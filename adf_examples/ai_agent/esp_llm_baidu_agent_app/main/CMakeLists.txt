idf_component_register(
    SRCS "app_main.c" "app_agent.c" "app_agent_utils.c"
    INCLUDE_DIRS "."
    REQUIRES
        baidu_app_media
        esp_event
        nvs_flash
        spiffs
        espressif__gmf_core
)

idf_component_optional_requires(PRIVATE json espressif__cjson)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
