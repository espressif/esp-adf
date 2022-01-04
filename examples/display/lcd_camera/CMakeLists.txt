# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(lcd_camera)

IF (((IDF_VERSION_MAJOR EQUAL 4) AND (IDF_VERSION_MINOR GREATER 0)) OR (IDF_VERSION_MAJOR GREATER 4))
idf_component_get_property(esp32_camera_lib espressif__esp32-camera COMPONENT_LIB)
target_compile_options(${esp32_camera_lib} PRIVATE "-Wno-unused-variable" "-Wno-unused-but-set-variable" "-Wno-unused-function")

ENDIF()
