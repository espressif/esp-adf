set(COMPONENT_SRCS "main.c" "volc_rtc.c" "coze_http_request.c" "volc_rtc_message.c" "http_client_handler.c")
set(COMPONENT_ADD_INCLUDEDIRS .)
register_component()

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
