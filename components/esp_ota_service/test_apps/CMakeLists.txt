#
# Unity test app: OTA tests under main/; SD is initialized via esp_board_manager (per board YAML).

cmake_minimum_required(VERSION 3.16)

get_filename_component(_ADF_COMPS "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${_ADF_COMPS}/components"
    "$ENV{IDF_PATH}/tools/unit-test-app/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp_ota_service_ut)
