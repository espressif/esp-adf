set(COMPONENT_ADD_INCLUDEDIRS ./include
./lib/adc_button
./lib/gpio_isr
./lib/sdcard
./lib/touch
./lib/button
./lib/blufi
./lib/IS31FL3216
./driver/i2c_bus
)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES driver  audio_hal esp_adc_cal audio_sal fatfs console audio_pipeline audio_board spiffs display_service esp_dispatcher bt mbedtls)
set(COMPONENT_PRIV_REQUIRES )

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
                ./lib/sdcard/sdcard.c
                ./lib/button/button.c
                ./lib/touch/touch.c
                ./lib/blufi/blufi_security.c
                ./lib/blufi/wifibleconfig.c
                ./lib/adc_button/adc_button.c
                ./lib/IS31FL3216/IS31FL3216.c
                ./driver/i2c_bus/i2c_bus.c
                ./lib/gpio_isr/gpio_isr.c)

register_component()
