set(srcs
    "src/core/esp_player_service.c"
    "src/core/esp_player_service_ctrl.c"
    "src/core/esp_player_service_setup.c"
    "src/core/esp_player_service_playlist.c"
    "src/core/esp_player_service_mix.c"
)

if(CONFIG_ESP_PLAYER_ENABLE_AUDIO)
    list(APPEND srcs "src/out/player_out_audio.c")
else()
    list(APPEND srcs "src/out/player_out_stub.c")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "private_inc"
)
