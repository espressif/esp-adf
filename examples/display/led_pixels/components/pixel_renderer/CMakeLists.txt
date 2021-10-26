set(COMPONENT_ADD_INCLUDEDIRS "include"
                              "./led_driver"
                              "./led_driver/ws2812_spi"
                              "./led_driver/ws2812_rmt")

set(COMPONENT_SRCS "pixel_renderer.c"
                   "pixel_coord.c"
                   "pixel_dev.c"
                   "./led_driver/ws2812_spi/ws2812_spi.c"
                   "./led_driver/ws2812_rmt/ws2812_rmt.c")
set(COMPONENT_REQUIRES utilis audio_sal)

register_component()