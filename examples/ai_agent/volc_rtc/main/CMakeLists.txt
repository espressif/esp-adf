set(COMPONENT_SRCS "main.c" "volc_rtc.c" "byte_rtc_request.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
