set(IDF_TARGET esp32)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.spiram
    boards/sdkconfig.base
    boards/sdkconfig.ble
    boards/sdkconfig.240mhz
    $ENV{ADF_PATH}/micropython_adf/sdkconfig.adf
    $ENV{ADF_PATH}/micropython_adf/boards/lyrat43/sdkconfig.lyrat43
)

set(ADF_COMPS     "$ENV{ADF_PATH}/components")
set(ADF_BOARD_DIR "$ENV{ADF_PATH}/components/audio_board/lyrat_v4_3")

set(ADF_BOARD_CODEC_SRC
            ${ADF_COMPS}/audio_hal/driver/es8388/es8388.c
            ${ADF_COMPS}/audio_hal/driver/es8388/headphone_detect.c)
set(ADF_BOARD_CODEC_INC
            ${ADF_COMPS}/audio_hal/driver/es8388)
set(ADF_BOARD_INIT_SRC
            $ENV{ADF_PATH}/components $ENV{ADF_PATH}/micropython_adf/boards/lyrat43/board_init.c)

list(APPEND EXTRA_COMPONENT_DIRS
        $ENV{ADF_PATH}/components/audio_pipeline
        $ENV{ADF_PATH}/components/audio_sal
        $ENV{ADF_PATH}/components/esp-adf-libs
        $ENV{ADF_PATH}/components/esp-sr
        $ENV{ADF_PATH}/micropython_adf/boards)
