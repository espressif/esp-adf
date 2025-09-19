set(COMPONENT_SRCS "main.c" "brtc_app.c" "basic_board.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
