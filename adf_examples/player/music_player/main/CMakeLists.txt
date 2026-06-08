idf_component_register(SRC_DIRS "./" INCLUDE_DIRS "./")

set(assets_dir "${CMAKE_CURRENT_LIST_DIR}/../assets")
if(EXISTS "${assets_dir}")
    spiffs_create_partition_assets(assets "${assets_dir}" FLASH_IN_PROJECT
                                   MMAP_FILE_SUPPORT_FORMAT ".ttf")
    add_dependencies(${COMPONENT_LIB} assets_assets_bin)
endif()
