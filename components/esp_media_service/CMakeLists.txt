set(srcs
    "src/esp_media_service.c"
    "src/esp_media_track_mngr.c"
    "src/esp_media_track.c"
    "src/esp_media_provider.c"
)

idf_component_register(
    SRCS              ${srcs}
    INCLUDE_DIRS      "include"
    PRIV_INCLUDE_DIRS "private_inc"
)
