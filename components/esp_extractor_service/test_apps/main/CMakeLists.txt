idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_extractor_service.c"
        "test_mcp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_extractor_service esp_media_service esp_service vfs
)
