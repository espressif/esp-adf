set(SRC_FILES "audio_processor.c")

if (CONFIG_ESP32_S3_KORVO2_V3_BOARD OR CONFIG_ESP32_S3_BOX_3_BOARD)
    list(APPEND  SRC_FILES "audio_stream_dual_microphones.c")
elseif (CONFIG_ESP32_S3_KORVO2L_V1_BOARD)
    list(APPEND  SRC_FILES "audio_stream_single_microphone.c")
else ()
    list(APPEND  SRC_FILES "audio_stream_single_microphone.c")
    message(WARNING "Please add supported hardware and select the type of microphone, default use single microphone")
endif()

set(COMPONENT_SRCS ${SRC_FILES})
set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_REQUIRES audio_recorder audio_stream audio_pipeline audio_hal esp_peripherals)

register_component()

if (CONFIG_ESP32_S3_KORVO2_V3_BOARD OR CONFIG_ESP32_S3_BOX_3_BOARD)
    add_definitions(-DENBALE_AUDIO_STREAM_DUAL_MIC)
elseif (CONFIG_ESP32_S3_KORVO2L_V1_BOARD)
    add_definitions(-DENABLE_AUDIO_STREAM_SINGLE_MIC)
else ()
    add_definitions(-DENABLE_AUDIO_STREAM_SINGLE_MIC)
endif()