#
# Unified esp_service test harness:
#   * IDF target build — driven by this file via `idf.py build`; produces a
#     firmware that exercises service_manager + MCP transports (HTTP / SSE /
#     WS / UART) on-device.  The mock services themselves live with the
#     usage example under ../examples/mock_services/components/.
#   * PC build         — use sub-project ./pc/ (`cmake -S pc -B build`),
#     which builds five standalone executables (see pc/README).

cmake_minimum_required(VERSION 3.16)

get_filename_component(_ESP_SERVICE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.."               ABSOLUTE)
get_filename_component(_MOCK_COMPS  "${_ESP_SERVICE_ROOT}/examples/mock_services/components" ABSOLUTE)

set(EXTRA_COMPONENT_DIRS
    "${_ESP_SERVICE_ROOT}"
    "${_MOCK_COMPS}"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp_service_ut)
