# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/esp32/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp32 -lesp32 $(IDF_PATH)/components/esp32/libhal.a -L$(IDF_PATH)/components/esp32/lib -lcore -lrtc -lnet80211 -lpp -lwpa -lsmartconfig -lcoexist -lwps -lwpa2 -lespnow -lphy -L $(IDF_PATH)/components/esp32/ld -T esp32_out.ld -u ld_include_panic_highint_hdl -T esp32.common.ld -T esp32.rom.ld -T esp32.peripherals.ld -T esp32.rom.spiram_incompatible_fns.ld
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/esp32/lib/libcore.a $(IDF_PATH)/components/esp32/lib/librtc.a $(IDF_PATH)/components/esp32/lib/libnet80211.a $(IDF_PATH)/components/esp32/lib/libpp.a $(IDF_PATH)/components/esp32/lib/libwpa.a $(IDF_PATH)/components/esp32/lib/libsmartconfig.a $(IDF_PATH)/components/esp32/lib/libcoexist.a $(IDF_PATH)/components/esp32/lib/libwps.a $(IDF_PATH)/components/esp32/lib/libwpa2.a $(IDF_PATH)/components/esp32/lib/libespnow.a $(IDF_PATH)/components/esp32/lib/libphy.a $(IDF_PATH)/components/esp32/ld/esp32.common.ld $(IDF_PATH)/components/esp32/ld/esp32.rom.ld $(IDF_PATH)/components/esp32/ld/esp32.peripherals.ld $(IDF_PATH)/components/esp32/ld/esp32.rom.spiram_incompatible_fns.ld
COMPONENT_SUBMODULES += $(IDF_PATH)/components/esp32/lib
COMPONENT_LIBRARIES += esp32
component-esp32-build: 
