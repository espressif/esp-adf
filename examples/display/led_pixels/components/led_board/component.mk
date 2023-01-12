#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifdef CONFIG_AUDIO_BOARD_CUSTOM

COMPONENT_ADD_INCLUDEDIRS += ./esp32_c3_devkitm_1
COMPONENT_SRCDIRS += ./esp32_c3_devkitm_1
endif
