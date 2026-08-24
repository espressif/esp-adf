idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_media_service.c"
        "ut_esp_media_dummy_service.c"
        "test_mcp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity
    WHOLE_ARCHIVE
)
