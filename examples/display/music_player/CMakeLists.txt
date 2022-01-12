# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

add_compile_options(-fdiagnostics-color=always)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(music_player)

IF (((IDF_VERSION_MAJOR EQUAL 4) AND (IDF_VERSION_MINOR GREATER 0)) OR (IDF_VERSION_MAJOR GREATER 4))
# As the upstream LVGL library has build warnings in esp-idf build system, this is only for temporarily workaround
# Will remove this workaround when upstream LVGL fixes the warnings in the next release
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE "-Wno-empty-body" "-Wno-strict-prototypes")

ENDIF()