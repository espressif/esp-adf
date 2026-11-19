#
# Unified esp_service test harness:
#   * IDF target build — driven by this file via `idf.py build`; produces a
#     firmware that exercises service_manager + MCP transports (HTTP / SSE /
#     WS / UART) on-device.  The mock services themselves live with the
#     usage example under ../examples/mock_services/components/.

cmake_minimum_required(VERSION 3.16)

option(ESP_SERVICE_TEST_FAULT_INJECTION "Build transport fault-injection tests" OFF)

get_filename_component(_ESP_SERVICE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.."               ABSOLUTE)
get_filename_component(_MOCK_COMPS  "${_ESP_SERVICE_ROOT}/examples/mock_services/components" ABSOLUTE)

set(EXTRA_COMPONENT_DIRS
    "${_ESP_SERVICE_ROOT}"
    "${_MOCK_COMPS}"
    "${_ESP_SERVICE_ROOT}/../esp_rtmp_service"
    "${_ESP_SERVICE_ROOT}/../esp_rtsp_service"
    "${_ESP_SERVICE_ROOT}/../esp_extractor_service"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Share the profile with IDF's separate dependency-scanning process.
idf_build_set_property(ESP_SERVICE_TEST_FAULT_INJECTION "${ESP_SERVICE_TEST_FAULT_INJECTION}")
project(esp_service_ut)
