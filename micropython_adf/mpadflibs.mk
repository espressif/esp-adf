ifdef CONFIG_ESP_LYRATD_MSC_V2_1_BOARD
APP_LD_ARGS += -L$(ADFCOMP)/audio_hal/driver/zl38063/firmware -lfirmware
endif

ifdef CONFIG_ESP_LYRATD_MSC_V2_2_BOARD
APP_LD_ARGS += -L$(ADFCOMP)/audio_hal/driver/zl38063/firmware -lfirmware
endif

AUDIO_LIBS := esp_processing esp_audio esp-amr esp-amrwbenc esp-aac esp-ogg-container esp-opus esp-tremor esp-flac esp_ssdp esp_upnp esp_sip esp-mp3 codec-utils

APP_LD_ARGS +=  -L$(ADFCOMP)/esp-adf-libs/esp_audio/lib/esp32 \
				-L$(ADFCOMP)/esp-adf-libs/esp_codec/lib/esp32 \
				-L$(ADFCOMP)/esp-adf-libs/recorder_engine/lib/esp32 \
				-L$(ADFCOMP)/esp-adf-libs/esp_ssdp/lib/esp32 \
				-L$(ADFCOMP)/esp-adf-libs/esp_upnp/lib/esp32 \
				-L$(ADFCOMP)/esp-adf-libs/esp_sip/lib/esp32 \
				$(addprefix -l,$(AUDIO_LIBS))