idf_component_register(
    SRCS "app_main.c" "app_agent.c" "app_cli.c" "libc_compat.c"
    INCLUDE_DIRS "."
    REQUIRES
        tongyi_app_media
        esp_cli_service
        esp_event
        nvs_flash
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u stderr")

idf_component_optional_requires(PRIVATE json espressif__cjson)
