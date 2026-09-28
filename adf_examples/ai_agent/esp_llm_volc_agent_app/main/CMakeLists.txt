idf_component_register(
    SRCS "app_main.c" "app_agent.c"
    INCLUDE_DIRS "."
    REQUIRES
        volc_app_media
        esp_event
        nvs_flash
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u stderr")
