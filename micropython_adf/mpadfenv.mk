# path to ADF and it's components
ifeq ($(ADF_PATH),)
$(info The ADF_PATH variable has not been set, please set it to the root of the esp-adf repository.)
$(error ADF_PATH not set)
endif

ESPADF = $(ADF_PATH)
ADFCOMP = $(ADF_PATH)/components
ADFSDKCONFIG = $(ADF_PATH)/micropython_adf/sdkconfig.adf
ESPCOMP_KCONFIGS += $(shell find $(ADFCOMP) -name Kconfig)
ESPCOMP_KCONFIGS_PROJBUILD += $(shell find $(ADFCOMP) -name Kconfig.projbuild)

ADF_VER = := $(shell git -C $(ESPADF) describe)

CFLAGS_COMMON += -Wno-sign-compare

include $(ADFSDKCONFIG)

SDKCONFIG += $(ADFSDKCONFIG)

# bluetooth_service clouds dueros_service esp_actions input_key_service playlist wifi_service
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/include
# different board support
ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/lyrat_v4_3
endif
ifdef CONFIG_ESP_LYRAT_V4_2_BOARD
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/lyrat_v4_2
endif
ifdef CONFIG_ESP_LYRATD_MSC_V2_1_BOARD
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/lyratd_msc_v2_1
endif
ifdef CONFIG_ESP_LYRATD_MSC_V2_2_BOARD
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/lyratd_msc_v2_2
endif
ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
INC_ESPCOMP += -I$(ADFCOMP)/audio_board/lyrat_mini_v1_1
endif
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/include
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/include
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/es8388
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/es8374
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/es8311
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/es7243
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/zl38063
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/zl38063/api_lib
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/zl38063/example_apps
INC_ESPCOMP += -I$(ADFCOMP)/audio_hal/driver/zl38063/firmware
INC_ESPCOMP += -I$(ADFCOMP)/audio_pipeline/include
INC_ESPCOMP += -I$(ADFCOMP)/audio_sal/include
INC_ESPCOMP += -I$(ADFCOMP)/audio_stream
INC_ESPCOMP += -I$(ADFCOMP)/audio_stream/include
INC_ESPCOMP += -I$(ADFCOMP)/display_service/include
INC_ESPCOMP += -I$(ADFCOMP)/display_service/led_indicator/include
INC_ESPCOMP += -I$(ADFCOMP)/display_service/led_bar/include
INC_ESPCOMP += -I$(ADFCOMP)/esp_dispatcher/include
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/include
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/driver/i2c_bus
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/adc_button
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/blufi
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/button
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/gpio_isr
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/IS31FL3216
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/sdcard
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/touch
INC_ESPCOMP += -I$(ADFCOMP)/esp_peripherals/lib/aw2013
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_audio/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_codec/include/codec
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_codec/include/processing
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/recorder_engine/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_ssdp/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_dlna/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_upnp/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/esp_sip/include
INC_ESPCOMP += -I$(ADFCOMP)/esp-adf-libs/audio_misc/include

INC_ESPCOMP += -I$(ESPCOMP)/esp_http_client/include
INC_ESPCOMP += -I$(ESPCOMP)/esp_http_client/lib/include
INC_ESPCOMP += -I$(ESPCOMP)/spiffs/include
INC_ESPCOMP += -I$(ESPCOMP)/spiffs/src
INC_ESPCOMP += -I$(ESPCOMP)/fatfs/src
INC_ESPCOMP += -I$(ESPCOMP)/esp_adc_cal/include
INC_ESPCOMP += -I$(ESPCOMP)/wear_levelling/include
INC_ESPCOMP += -I$(ESPCOMP)/wear_levelling/private_include
INC_ESPCOMP += -I$(ESPCOMP)/tcp_transport/include
INC_ESPCOMP += -I$(ESPCOMP)/esp-tls
INC_ESPCOMP += -I$(ESPCOMP)/nghttp/port/include
