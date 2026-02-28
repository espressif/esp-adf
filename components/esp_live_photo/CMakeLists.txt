idf_component_register(SRCS "src/esp_live_photo_muxer.c" "src/esp_live_photo_extractor.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_muxer esp_extractor)
