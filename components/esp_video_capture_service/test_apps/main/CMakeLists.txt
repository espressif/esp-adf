idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_video_capture_service.c"
        "test_mcp.c"
        "../../../esp_capture_service/test_apps/main/capture_fake_aud_src.c"
        "../../../esp_capture_service/test_apps/main/capture_fake_vid_src.c"
    INCLUDE_DIRS
        "."
        "../../../esp_capture_service/test_apps/main"
    PRIV_REQUIRES unity
)
