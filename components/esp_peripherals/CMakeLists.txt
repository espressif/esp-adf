set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

list(APPEND COMPONENT_ADD_INCLUDEDIRS ./include
                ./lib/adc_button
                ./lib/gpio_isr
                ./lib/button
                ./lib/blufi
                ./lib/IS31FL3216
                ./lib/aw2013
                ./lib/tca9554
                ./driver/i2c_bus)

list(APPEND COMPONENT_SRCS ./esp_peripherals.c
                ./periph_adc_button.c
                ./periph_button.c
                ./periph_console.c
                ./periph_gpio_isr.c
                ./periph_is31fl3216.c
                ./periph_led.c
                ./periph_spiffs.c
                ./periph_wifi.c
                ./periph_aw2013.c
                ./periph_ws2812.c
                ./periph_lcd.c
                ./lib/button/button.c
                ./lib/blufi/blufi_security.c
                ./lib/blufi/wifibleconfig.c
                ./lib/adc_button/adc_button.c
                ./lib/IS31FL3216/IS31FL3216.c
                ./lib/tca9554/tca9554.c
                ./lib/gpio_isr/gpio_isr.c)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.3")
    list(APPEND COMPONENT_SRCS  ./driver/i2c_bus/i2c_bus.c)
else()
    list(APPEND COMPONENT_SRCS  ./driver/i2c_bus/i2c_bus_v2.c)
endif()

IF (CONFIG_IDF_TARGET STREQUAL "esp32")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard ./lib/touch)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c ./periph_touch.c ./lib/touch/touch.c)

ELSEIF (CONFIG_IDF_TARGET STREQUAL "esp32c3")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c)

ELSEIF (CONFIG_IDF_TARGET STREQUAL "esp32c6")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c)

ELSEIF (CONFIG_IDF_TARGET STREQUAL "esp32s3")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard ./lib/touch)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c ./periph_touch.c ./lib/touch/touch.c)

ELSEIF (CONFIG_IDF_TARGET STREQUAL "esp32s2")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard ./lib/touch)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c ./periph_touch.c ./lib/touch/touch.c)

ELSEIF (CONFIG_IDF_TARGET STREQUAL "esp32p4")
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./lib/sdcard)
list(APPEND COMPONENT_SRCS ./lib/sdcard/sdcard.c ./periph_sdcard.c)
ENDIF ()

set(COMPONENT_REQUIRES driver audio_hal audio_sal fatfs console audio_pipeline audio_board spiffs display_service esp_dispatcher bt mbedtls wpa_supplicant nvs_flash)

IF (((IDF_VERSION_MAJOR EQUAL 4) AND (IDF_VERSION_MINOR GREATER 3)) OR (IDF_VERSION_MAJOR GREATER 4))
list(APPEND COMPONENT_REQUIRES esp_lcd)
ENDIF()

if (idf_version VERSION_GREATER_EQUAL "5.0")
list(APPEND COMPONENT_REQUIRES esp_adc)
else()
list(APPEND COMPONENT_REQUIRES esp_adc_cal)
endif()

register_component()
