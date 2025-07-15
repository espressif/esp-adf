
set(COMPONENT_PRIV_INCLUDEDIRS "device/zl38063/api_lib" "device/zl38063/firmware" "device/priv_include")

set(COMPONENT_ADD_INCLUDEDIRS include interface device/include)

# set conversion sources
set(COMPONENT_SRCS
  esp_codec_dev.c
  esp_codec_dev_vol.c
  esp_codec_dev_if.c
  audio_codec_sw_vol.c
)

list(APPEND COMPONENT_SRCS
  platform/audio_codec_gpio.c
  platform/audio_codec_ctrl_i2c.c
  platform/audio_codec_data_i2s.c
  platform/audio_codec_ctrl_spi.c
  platform/esp_codec_dev_os.c
)

if (CONFIG_CODEC_ES8311_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es8311/es8311.c)
endif()

if (CONFIG_CODEC_ES8156_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es8156/es8156.c)
endif()

if (CONFIG_CODEC_ES7243E_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es7243e/es7243e.c)
endif()

if (CONFIG_CODEC_ES7210_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es7210/es7210.c)
endif()

if (CONFIG_CODEC_ES7243_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es7243/es7243.c)
endif()

if (CONFIG_CODEC_ES8388_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es8388/es8388.c)
endif()

if (CONFIG_CODEC_TAS5805M_SUPPORT)
  list(APPEND COMPONENT_SRCS device/tas5805m/tas5805m.c)
endif()

if (CONFIG_CODEC_ES8374_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es8374/es8374.c)
endif()

if (CONFIG_CODEC_AW88298_SUPPORT)
  list(APPEND COMPONENT_SRCS device/aw88298/aw88298.c)
endif()

if (CONFIG_CODEC_ES8389_SUPPORT)
  list(APPEND COMPONENT_SRCS device/es8389/es8389.c)
endif()

if (CONFIG_CODEC_ZL38063_SUPPORT)
  list(APPEND COMPONENT_SRCS device/zl38063/zl38063.c
    device/zl38063/api_lib/vprocTwolf_access.c
    device/zl38063/api_lib/vproc_common.c
    device/zl38063/example_apps/tw_hal_verify.c
    device/zl38063/example_apps/tw_ldcfg.c
    device/zl38063/example_apps/tw_ldfw.c
    device/zl38063/example_apps/tw_ldfwcfg.c
    device/zl38063/example_apps/tw_spi_access.c)
endif()

if (CONFIG_CODEC_CJC8910_SUPPORT)
  list(APPEND COMPONENT_SRCS device/cjc8910/cjc8910.c)
endif()

idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                       PRIV_INCLUDE_DIRS "${COMPONENT_PRIV_INCLUDEDIRS}"
                       REQUIRES driver
                       PRIV_REQUIRES freertos)
# Library only support xtensa
if (CONFIG_CODEC_ZL38063_SUPPORT)
  if (NOT ((CONFIG_IDF_TARGET STREQUAL "esp32c6") OR (CONFIG_IDF_TARGET STREQUAL "esp32c3") OR (CONFIG_IDF_TARGET STREQUAL "esp32p4")))
    target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${CMAKE_CURRENT_LIST_DIR}/device/zl38063/firmware")
    target_link_libraries(${COMPONENT_TARGET} INTERFACE firmware)
  endif()
endif()
