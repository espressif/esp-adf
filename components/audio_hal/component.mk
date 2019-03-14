#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include ./board/include 

COMPONENT_SRCDIRS := . ./board

ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./driver/es8388  ./board/lyrat_v4_3
COMPONENT_SRCDIRS += ./driver/es8388 ./board/lyrat_v4_3
endif

ifdef CONFIG_ESP_LYRAT_V4_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./driver/es8388 ./board/lyrat_v4_2
COMPONENT_SRCDIRS += ./driver/es8388 ./board/lyrat_v4_2
endif

ifdef CONFIG_ESP_LYRATD_MSC_V2_1_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware ./board/lyratd_msc_v2_1
COMPONENT_SRCDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware ./board/lyratd_msc_v2_1
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/driver/zl38063/firmware -lfirmware
endif

ifdef CONFIG_ESP_LYRATD_MSC_V2_2_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware ./board/lyratd_msc_v2_2
COMPONENT_SRCDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware ./board/lyratd_msc_v2_2
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/driver/zl38063/firmware -lfirmware
endif


