set(COMPONENT_SRCS "play_spiffs_mp3_example.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
