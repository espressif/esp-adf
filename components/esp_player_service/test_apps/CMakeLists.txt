cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
)

set(_PARTITIONS_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.partitions")
if(EXISTS "${_PARTITIONS_DEFAULTS}")
    list(APPEND SDKCONFIG_DEFAULTS "${_PARTITIONS_DEFAULTS}")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/.."
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_player_service_test_app)
