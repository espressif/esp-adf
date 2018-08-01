# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/freertos/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/freertos -lfreertos -Wl,--undefined=uxTopUsedPriority
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += freertos
component-freertos-build: 
