set(COMPONENT_SRCS "duer_audio_wrapper.c" "dueros_app.c" "app_main.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)