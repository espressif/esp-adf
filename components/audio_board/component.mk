#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include

ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./lyrat_v4_3
COMPONENT_SRCDIRS += ./lyrat_v4_3
endif

ifdef CONFIG_ESP_LYRAT_V4_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./lyrat_v4_2
COMPONENT_SRCDIRS += ./lyrat_v4_2
endif

ifdef CONFIG_ESP_LYRATD_MSC_V2_1_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./lyratd_msc_v2_1
COMPONENT_SRCDIRS += ./lyratd_msc_v2_1
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/../audio_hal/driver/zl38063/firmware -lfirmware
endif

ifdef CONFIG_ESP_LYRATD_MSC_V2_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./lyratd_msc_v2_2
COMPONENT_SRCDIRS += ./lyratd_msc_v2_2
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/../audio_hal/driver/zl38063/firmware -lfirmware
endif

ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./lyrat_mini_v1_1
COMPONENT_SRCDIRS += ./lyrat_mini_v1_1
endif

ifdef CONFIG_ESP32_KORVO_DU1906_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./esp32_korvo_du1906
COMPONENT_SRCDIRS += ./esp32_korvo_du1906
endif

ifdef CONFIG_ESP32_S2_KALUGA_1_V1_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./esp32_s2_kaluga_1_v1_2
COMPONENT_SRCDIRS += ./esp32_s2_kaluga_1_v1_2
endif

ifdef CONFIG_ESP_AI_THINKER_V2_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./ai_thinker_audio_kit_v2_2
COMPONENT_SRCDIRS += ./ai_thinker_audio_kit_v2_2
endif
