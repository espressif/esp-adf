set(COMPONENT_SRCS "fatfs_stream.c"
                    "i2s_stream.c"
                    "http_stream.c"
                    "http_playlist.c"
                    "raw_stream.c"
                    "spiffs_stream.c"
                    "tone_stream.c"
                    "tcp_client_stream.c"
                    "embed_flash_stream.c"
                    "pwm_stream.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_PRIV_INCLUDEDIRS "lib/hls/include")
list(APPEND COMPONENT_SRCS  "lib/hls/hls_parse.c"
                            "lib/hls/hls_playlist.c"
                            "lib/hls/line_reader.c"
                            "lib/hls/join_path.c")

set(COMPONENT_REQUIRES audio_pipeline audio_sal esp_http_client tcp_transport spiffs esp-adf-libs audio_board bootloader_support esp_dispatcher esp_actions tone_partition)

if((${IDF_TARGET} STREQUAL "esp32") OR (${IDF_TARGET} STREQUAL "esp32s3"))
    list(APPEND COMPONENT_SRCS "algorithm_stream.c" "tts_stream.c")
    list(APPEND COMPONENT_REQUIRES esp-sr)
endif()

register_component()
