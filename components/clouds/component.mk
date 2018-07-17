#
# Component Makefile
#

# DuerOS LightDuer Module
LIGHTDUER_PATH := dueros/lightduer

COMPONENT_ADD_INCLUDEDIRS += $(LIGHTDUER_PATH)/include

COMPONENT_SRCDIRS :=

COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/$(LIGHTDUER_PATH) -lduer-device
