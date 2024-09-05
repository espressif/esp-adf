set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_SRCS "recorder_encoder.c" "audio_recorder.c")

set(COMPONENT_REQUIRES audio_sal audio_pipeline)

if((${IDF_TARGET} STREQUAL "esp32") OR (${IDF_TARGET} STREQUAL "esp32s3") OR (${IDF_TARGET} STREQUAL "esp32p4"))
    list(APPEND COMPONENT_SRCS "recorder_sr.c")
    list(APPEND COMPONENT_REQUIRES esp-sr)
endif()

if (idf_version VERSION_GREATER_EQUAL "5.0")
list(APPEND COMPONENT_REQUIRES esp_timer)
endif()

register_component()
