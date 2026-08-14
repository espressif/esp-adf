cmake_minimum_required(VERSION 3.16)

# Nested mock wifi_service has no idf_component.yml
get_filename_component(_MOCK_WIFI_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../components/esp_service/examples/mock_services/components/wifi_service"
    ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${_MOCK_WIFI_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(services_hub)
