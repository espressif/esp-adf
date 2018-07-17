# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/coap/port/include $(IDF_PATH)/components/coap/port/include/coap $(IDF_PATH)/components/coap/libcoap/include $(IDF_PATH)/components/coap/libcoap/include/coap
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/coap -lcoap
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += $(IDF_PATH)/components/coap/libcoap
COMPONENT_LIBRARIES += coap
component-coap-build: 
