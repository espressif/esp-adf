set(COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_SRCS "recorder_encoder.c" "audio_recorder.c")

set(COMPONENT_REQUIRES audio_sal audio_pipeline)

if((${IDF_TARGET} STREQUAL "esp32") OR (${IDF_TARGET} STREQUAL "esp32s3"))
    list(APPEND COMPONENT_SRCS "recorder_sr.c")
    list(APPEND COMPONENT_REQUIRES esp-sr)
endif()

register_component()
