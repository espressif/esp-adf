cmake_minimum_required(VERSION 3.16)

get_filename_component(_ADF_COMPS "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${_ADF_COMPS}/components/esp_wifi_service"
    "${_ADF_COMPS}/components/esp_config_manager"
    "${_ADF_COMPS}/components/esp_service"
    "${_ADF_COMPS}/components/adf_event_hub"
)
if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
elseif(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components/test_utils")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/test_apps/components/test_utils")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp_wifi_service_ut)
