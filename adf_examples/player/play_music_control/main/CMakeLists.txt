idf_component_register(SRC_DIRS "./" INCLUDE_DIRS "./")

littlefs_create_partition_image(storage ../tone FLASH_IN_PROJECT)
