cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../esp_cli_service")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_cli_service_example)
