idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_muxer_service.c"
        "test_mcp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_muxer_service esp_media_service esp_service media_lib_sal
)
