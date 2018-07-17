# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/json/cJSON
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/json -ljson
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += $(IDF_PATH)/components/json/cJSON
COMPONENT_LIBRARIES += json
component-json-build: 
