# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/main/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/main -lmain -L $(PROJECT_PATH)/main -T esp32.bootloader.ld -T $(IDF_PATH)/components/esp32/ld/esp32.rom.ld -T $(IDF_PATH)/components/esp32/ld/esp32.rom.spiram_incompatible_fns.ld -T $(IDF_PATH)/components/esp32/ld/esp32.peripherals.ld -T esp32.bootloader.rom.ld
COMPONENT_LINKER_DEPS += $(PROJECT_PATH)/main/esp32.bootloader.ld $(IDF_PATH)/components/esp32/ld/esp32.rom.ld $(IDF_PATH)/components/esp32/ld/esp32.rom.spiram_incompatible_fns.ld $(IDF_PATH)/components/esp32/ld/esp32.peripherals.ld $(PROJECT_PATH)/main/esp32.bootloader.rom.ld
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += main
component-main-build: 
