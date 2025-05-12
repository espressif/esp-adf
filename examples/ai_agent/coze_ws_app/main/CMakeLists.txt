idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./"
                       REQUIRES )

if (NOT CONFIG_KEY_PRESS_DIALOG_MODE)
spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
endif()
