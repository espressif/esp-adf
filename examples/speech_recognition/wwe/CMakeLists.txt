# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options (-fdiagnostics-color=always)

project(example_wwe)

# This is a cmake function, which is used to flash the bin file to the specified partition
function(esptool_py_flash_customize_image target_name image_name offset image)
    idf_build_get_property(build_dir BUILD_DIR)
    file(RELATIVE_PATH image ${build_dir} ${image})

    set_property(TARGET ${target_name} APPEND PROPERTY FLASH_FILE
                "\"${offset}\" : \"${image}\"")
    set_property(TARGET ${target_name} APPEND PROPERTY FLASH_ENTRY
                "\"${image_name}\" : { \"offset\" : \"${offset}\", \"file\" : \"${image}\" }")
    set_property(TARGET ${target_name} APPEND PROPERTY IMAGES "${offset} ${image}")

    if(CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT)
        set_property(TARGET encrypted-${target_name} APPEND PROPERTY FLASH_FILE
                    "\"${offset}\" : \"${image}\"")
        set_property(TARGET encrypted-${target_name} APPEND PROPERTY FLASH_ENTRY
                    "\"${image_name}\" : { \"offset\" : \"${offset}\", \"file\" : \"${image}\" }")
        set_property(TARGET encrypted-${target_name} APPEND PROPERTY IMAGES "${offset} ${image}")
    endif()
endfunction()


# Flash the custom partition named `flash_tone`.
set(partition flash_tone)
idf_build_get_property(project_dir PROJECT_DIR)
set(image_file ${project_dir}/tone/audio_tone.bin)
partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")
esptool_py_flash_customize_image(flash "${partition}" "${offset}" "${image_file}")