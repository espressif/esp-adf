IF ( NOT (CONFIG_IDF_TARGET MATCHES "esp32s2"))
set(COMPONENT_ADD_INCLUDEDIRS ./include
./lib/adc_button
./lib/gpio_isr
./lib/sdcard
./lib/touch
./lib/button
./lib/blufi
./lib/IS31FL3216
./lib/aw2013
./driver/i2c_bus
)
ELSEIF (CONFIG_IDF_TARGET MATCHES "esp32s2")
set(COMPONENT_ADD_INCLUDEDIRS ./include
./lib/adc_button
./lib/gpio_isr
./lib/touch
./lib/button
./lib/blufi
./lib/IS31FL3216
./lib/aw2013
./driver/i2c_bus
)
ENDIF ( NOT (CONFIG_IDF_TARGET MATCHES "esp32s2"))


# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES driver  audio_hal esp_adc_cal audio_sal fatfs console audio_pipeline audio_board spiffs display_service esp_dispatcher bt mbedtls wpa_supplicant)
set(COMPONENT_PRIV_REQUIRES )
IF ( NOT (CONFIG_IDF_TARGET MATCHES "esp32s2"))
set(COMPONENT_SRCS ./esp_peripherals.c
                ./periph_adc_button.c
                ./periph_button.c
                ./periph_console.c
                ./periph_gpio_isr.c
                ./periph_is31fl3216.c
                ./periph_led.c
                ./periph_sdcard.c
                ./periph_spiffs.c
                ./periph_touch.c
                ./periph_wifi.c
                ./periph_ws2812.c
                ./periph_aw2013.c
                ./lib/sdcard/sdcard.c
                ./lib/button/button.c
                ./lib/touch/touch.c
                ./lib/blufi/blufi_security.c
                ./lib/blufi/wifibleconfig.c
                ./lib/adc_button/adc_button.c
                ./lib/IS31FL3216/IS31FL3216.c
                ./driver/i2c_bus/i2c_bus.c
                ./lib/gpio_isr/gpio_isr.c)

ELSEIF (CONFIG_IDF_TARGET MATCHES "esp32s2")
set(COMPONENT_SRCS ./esp_peripherals.c
                ./periph_adc_button.c
                ./periph_button.c
                ./periph_console.c
                ./periph_gpio_isr.c
                ./periph_is31fl3216.c
                ./periph_led.c
                ./periph_spiffs.c
                ./periph_touch.c
                ./periph_wifi.c
                ./periph_aw2013.c
                ./lib/button/button.c
                ./lib/touch/touch.c
                ./lib/blufi/blufi_security.c
                ./lib/blufi/wifibleconfig.c
                ./lib/adc_button/adc_button.c
                ./lib/IS31FL3216/IS31FL3216.c
                ./driver/i2c_bus/i2c_bus.c
                ./lib/gpio_isr/gpio_isr.c)
ENDIF ( NOT (CONFIG_IDF_TARGET MATCHES "esp32s2"))

register_component()
