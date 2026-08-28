idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_rtmp_service.c"
        "test_mcp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_timer esp_wifi nvs_flash
)
