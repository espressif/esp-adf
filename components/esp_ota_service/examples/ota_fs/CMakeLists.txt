cmake_minimum_required(VERSION 3.16)

get_filename_component(_ADF_COMPONENTS "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${_ADF_COMPONENTS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ota_fs)
