idf_component_register(
    SRCS
        "app_main.c"
        "video_player_board.c"
        "ut_esp_video_player_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity fatfs espressif__esp_board_manager
)
