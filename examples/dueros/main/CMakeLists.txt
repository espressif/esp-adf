idf_component_register(SRCS "duer_audio_wrapper.c" "dueros_app.c" "dueros_handler.c" "app_main.c"
                       INCLUDE_DIRS "."
                       WHOLE_ARCHIVE)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)