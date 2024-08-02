set(COMPONENT_ADD_INCLUDEDIRS ./include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_peripherals audio_sal audio_hal esp_dispatcher display_service)


if (CONFIG_ESP_LYRAT_V4_2_BOARD)
message(STATUS "Current board name is " CONFIG_ESP_LYRAT_V4_2_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lyrat_v4_2)
set(COMPONENT_SRCS
./lyrat_v4_2/board.c
./lyrat_v4_2/board_pins_config.c
)
endif()

if (CONFIG_ESP_LYRAT_V4_3_BOARD)
message(STATUS "Current board name is " CONFIG_ESP_LYRAT_V4_3_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lyrat_v4_3)
set(COMPONENT_SRCS
./lyrat_v4_3/board.c
./lyrat_v4_3/board_pins_config.c
)
endif()

if (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
message(STATUS "Current board name is " CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lyrat_mini_v1_1)
set(COMPONENT_SRCS
./lyrat_mini_v1_1/board.c
./lyrat_mini_v1_1/board_pins_config.c
)
endif()


if (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD)
message(STATUS "Current board name is " CONFIG_ESP_LYRATD_MSC_V2_1_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lyratd_msc_v2_1)
set(COMPONENT_SRCS
./lyratd_msc_v2_1/board.c
./lyratd_msc_v2_1/board_pins_config.c
)
endif()

if (CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
message(STATUS "Current board name is " CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lyratd_msc_v2_2)
set(COMPONENT_SRCS
./lyratd_msc_v2_2/board.c
./lyratd_msc_v2_2/board_pins_config.c
)
endif()

if (CONFIG_ESP32_KORVO_DU1906_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_KORVO_DU1906_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_korvo_du1906)
set(COMPONENT_SRCS
./esp32_korvo_du1906/board.c
./esp32_korvo_du1906/board_pins_config.c
./esp32_korvo_du1906/du1906_bar_pattern.c
)
endif()

if (CONFIG_ESP32_S2_KALUGA_1_V1_2_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S2_KALUGA_1_V1_2_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s2_kaluga_1_v1_2)
set(COMPONENT_SRCS
./esp32_s2_kaluga_1_v1_2/board.c
./esp32_s2_kaluga_1_v1_2/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_KORVO2_V3_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_KORVO2_V3_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_korvo2_v3)
set(COMPONENT_SRCS
./esp32_s3_korvo2_v3/board.c
./esp32_s3_korvo2_v3/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_KORVO2L_V1_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_KORVO2L_V1_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_korvo2l_v1)
set(COMPONENT_SRCS
./esp32_s3_korvo2l_v1/board.c
./esp32_s3_korvo2l_v1/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_BOX_LITE_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_BOX_LITE_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_box_lite)
set(COMPONENT_SRCS
./esp32_s3_box_lite/board.c
./esp32_s3_box_lite/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_BOX_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_BOX_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_box)
set(COMPONENT_SRCS
./esp32_s3_box/board.c
./esp32_s3_box/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_BOX_3_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_BOX_3_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_box_3)
set(COMPONENT_SRCS
./esp32_s3_box_3/board.c
./esp32_s3_box_3/board_pins_config.c
)
endif()

if (CONFIG_ESP32_C3_LYRA_V2_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_C3_LYRA_V2_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_c3_lyra)
set(COMPONENT_SRCS
./esp32_c3_lyra/board.c
./esp32_c3_lyra/board_pins_config.c
./esp32_c3_lyra/C3_lyra_sys_pattern.c
)
endif()

if (CONFIG_ESP32_C6_DEVKIT_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_C6_DEVKIT_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_c6_devkit)
set(COMPONENT_SRCS
./esp32_c6_devkit/board.c
./esp32_c6_devkit/board_pins_config.c
)
endif()

if (CONFIG_ESP32_P4_FUNCTION_EV_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_P4_FUNCTION_EV_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_p4_function_ev_board)
set(COMPONENT_SRCS
./esp32_p4_function_ev_board/board.c
./esp32_p4_function_ev_board/board_pins_config.c
)
endif()

register_component()
