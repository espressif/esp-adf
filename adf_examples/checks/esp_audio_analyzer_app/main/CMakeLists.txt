set(src_dirs                "./")

set(public_include_dirs     "./")

if (CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH)
    include(${PROJECT_DIR}/components/embed_audio_file/esp_embed_tone.cmake)
    list(APPEND public_include_dirs ${PROJECT_DIR}/components/embed_audio_file/)
endif()

idf_component_register(SRC_DIRS "${src_dirs}"
                       INCLUDE_DIRS "${public_include_dirs}"
                       EMBED_FILES ${COMPONENT_EMBED_TXTFILES}
                       EMBED_TXTFILES "certs/servercert.pem" "certs/prvtkey.pem")
