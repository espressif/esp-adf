idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./"
                       REQUIRES )

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)