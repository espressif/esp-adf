cmake_minimum_required(VERSION 3.16)

if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()
if(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/test_apps/components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_ota_service_ut)
