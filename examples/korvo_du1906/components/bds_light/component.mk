#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS := .
COMPONENT_ADD_INCLUDEDIRS := include 
ifdef CONFIG_AUDIO_BOARD_CUSTOM
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/lib -lbds_light_sdk_cupid
else
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/lib -lbds_light_sdk
endif
