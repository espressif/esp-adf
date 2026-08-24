idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_capture_service.c"
        "capture_fake_aud_src.c"
        "capture_fake_vid_src.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES unity
)
