# This is the project CMakeLists.txt file for the test subproject 
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS  "$ENV{IDF_PATH}/tools/unit-test-app/components"
                           "../")

add_compile_options(-fdiagnostics-color=always)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{ADF_PATH}/CMakeLists.txt)
project(test_app)
