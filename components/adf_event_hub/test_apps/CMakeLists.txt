cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../adf_event_hub"
)

if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()
if(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/test_apps/components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(adf_event_hub_ut)
