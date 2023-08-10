set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.usb
    boards/sdkconfig.ble
    boards/sdkconfig.240mhz
    boards/sdkconfig.spiram_sx
    boards/sdkconfig.spiram_oct
    $ENV{ADF_PATH}/micropython_adf/sdkconfig.adf
    $ENV{ADF_PATH}/micropython_adf/boards/korvo2v3/sdkconfig.korvo2v3
)

set(ADF_COMPS     "$ENV{ADF_PATH}/components")
set(ADF_BOARD_DIR "$ENV{ADF_PATH}/components/audio_board/esp32_s3_korvo2_v3")

set(ADF_BOARD_CODEC_SRC
            ${ADF_COMPS}/audio_hal/driver/es7210/es7210.c
            ${ADF_COMPS}/audio_hal/driver/es8311/es8311.c)
set(ADF_BOARD_CODEC_INC
            ${ADF_COMPS}/audio_hal/driver/es7210
            ${ADF_COMPS}/audio_hal/driver/es8311)
set(ADF_BOARD_INIT_SRC
            $ENV{ADF_PATH}/components $ENV{ADF_PATH}/micropython_adf/boards/korvo2v3/board_init.c)

list(APPEND EXTRA_COMPONENT_DIRS
        $ENV{ADF_PATH}/components/audio_pipeline
        $ENV{ADF_PATH}/components/audio_sal
        $ENV{ADF_PATH}/components/esp-adf-libs
        $ENV{ADF_PATH}/components/esp-sr
        $ENV{ADF_PATH}/micropython_adf/boards)
