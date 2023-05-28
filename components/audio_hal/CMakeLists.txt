set(COMPONENT_ADD_INCLUDEDIRS
    ./include
    ./driver/include
)

set(COMPONENT_SRCS
    ./audio_hal.c
    ./audio_volume.c
)

IF (CONFIG_CODEC_ES8311_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es8311)
    list(APPEND COMPONENT_SRCS ./driver/es8311/es8311.c)
endif()

IF (CONFIG_CODEC_ES7210_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es7210)
    list(APPEND COMPONENT_SRCS ./driver/es7210/es7210.c)
endif()

IF (CONFIG_CODEC_ES7243_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es7243)
    list(APPEND COMPONENT_SRCS ./driver/es7243/es7243.c)
endif()

IF (CONFIG_CODEC_ES7243E_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es7243e)
    list(APPEND COMPONENT_SRCS ./driver/es7243e/es7243e.c)
endif()

IF (CONFIG_CODEC_ES8156_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es8156)
    list(APPEND COMPONENT_SRCS ./driver/es8156/es8156.c)
endif()

IF (CONFIG_CODEC_ES8374_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es8374)
    list(APPEND COMPONENT_SRCS ./driver/es8374/es8374.c)
endif()

IF (CONFIG_CODEC_ES8388_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es8388)
    list(APPEND COMPONENT_SRCS ./driver/es8388/es8388.c ./driver/es8388/headphone_detect.c)
endif()

IF (CONFIG_CODEC_ES7148_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/es7148)
    list(APPEND COMPONENT_SRCS ./driver/es7148/es7148.c)
endif()

IF (CONFIG_CODEC_TAS5805M_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ./driver/tas5805m)
    list(APPEND COMPONENT_SRCS ./driver/tas5805m/tas5805m.c)
endif()

IF (CONFIG_CODEC_ZL38063_SUPPORT)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS 
        ./driver/zl38063
        ./driver/zl38063/api_lib
        ./driver/zl38063/example_apps
        ./driver/zl38063/firmware
    )

    list(APPEND COMPONENT_SRCS 
        ./driver/zl38063/zl38063.c
        ./driver/zl38063/api_lib/vprocTwolf_access.c
        ./driver/zl38063/api_lib/vproc_common.c
        ./driver/zl38063/example_apps/tw_hal_verify.c
        ./driver/zl38063/example_apps/tw_ldcfg.c
        ./driver/zl38063/example_apps/tw_ldfw.c
        ./driver/zl38063/example_apps/tw_ldfwcfg.c
        ./driver/zl38063/example_apps/tw_spi_access.c
    )
endif()

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES audio_sal audio_board mbedtls esp_peripherals display_service esp_dispatcher)

register_component()

IF (CONFIG_CODEC_ZL38063_SUPPORT)
    target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${CMAKE_CURRENT_LIST_DIR}/driver/zl38063/firmware")
    target_link_libraries(${COMPONENT_TARGET} INTERFACE firmware)
ENDIF()
