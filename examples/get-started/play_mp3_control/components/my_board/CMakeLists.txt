# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES audio_sal audio_hal esp_dispatcher esp_peripherals display_service)

if(CONFIG_AUDIO_BOARD_CUSTOM)
message(STATUS "Current board name is " CONFIG_AUDIO_BOARD_CUSTOM)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./my_board_v1_0 ./my_codec_driver)
set(COMPONENT_SRCS
./my_board_v1_0/board.c
./my_board_v1_0/board_pins_config.c
./my_codec_driver/new_codec.c
)
endif()

register_component()

IF (IDF_VER MATCHES "v4.")
idf_component_get_property(audio_board_lib audio_board COMPONENT_LIB)
set_property(TARGET ${audio_board_lib} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${COMPONENT_LIB})

ELSEIF (IDF_VER MATCHES "v3.")
set_property(TARGET idf_component_audio_board APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES  $<TARGET_PROPERTY:${COMPONENT_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

ENDIF (IDF_VER MATCHES "v4.")
