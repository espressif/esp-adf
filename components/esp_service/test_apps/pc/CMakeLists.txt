cmake_minimum_required(VERSION 3.16)
project(esp_service_pc_tests C)

# PC-side test/sim harness for the esp_service framework.
#
# Builds five executables from one tree:
#   service_sim_example          — multi-service event-hub simulation
#   service_tool_dispatch_test   — service_manager tool dispatch coverage
#   service_integration_test     — three interaction modes coexisting
#   service_coverage_test        — boundary / stress / multi-thread suite
#   mcp_http_server              — long-running MCP HTTP JSON-RPC server
#
# Mock services themselves live alongside their demo in
# `../../examples/mock_services/components/` and are consumed via EXAMPLE_COMPS_DIR
# below, so the example and the tests stay in sync.

include(FetchContent)

# ── FreeRTOS Kernel (POSIX/GCC simulator) ─────────────────────────────
set(FREERTOS_HEAP     "3"         CACHE STRING "" FORCE)
set(FREERTOS_PORT     "GCC_POSIX" CACHE STRING "" FORCE)
set(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "" FORCE)

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.2.0
    GIT_SHALLOW    TRUE
)

# ── cJSON library ────────────────────────────────────────────────────
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(freertos_kernel)
FetchContent_Populate(cjson)

# Suppress macOS deprecation warnings for setitimer in the POSIX port
if(APPLE)
    target_compile_options(freertos_kernel PRIVATE -Wno-deprecated-declarations)
endif()

# ── cJSON as a static library (just the single .c file) ─────────────
add_library(cjson STATIC ${cjson_SOURCE_DIR}/cJSON.c)
target_include_directories(cjson PUBLIC ${cjson_SOURCE_DIR})

# ── Create "freertos/" prefixed include wrapper ──────────────────────
# ESP-IDF code uses #include "freertos/FreeRTOS.h" (with prefix).
# FreeRTOS-Kernel puts headers directly in include/ (no prefix).
set(FREERTOS_WRAPPER_DIR "${CMAKE_BINARY_DIR}/freertos_wrapper/freertos")
file(MAKE_DIRECTORY "${FREERTOS_WRAPPER_DIR}")

set(FREERTOS_INCLUDE_DIR "${freertos_kernel_SOURCE_DIR}/include")
foreach(hdr FreeRTOS.h task.h queue.h semphr.h timers.h list.h
            event_groups.h stream_buffer.h message_buffer.h
            projdefs.h portable.h mpu_wrappers.h stack_macros.h
            mpu_prototypes.h)
    if(EXISTS "${FREERTOS_INCLUDE_DIR}/${hdr}")
        file(CREATE_LINK "${FREERTOS_INCLUDE_DIR}/${hdr}"
             "${FREERTOS_WRAPPER_DIR}/${hdr}" SYMBOLIC)
    endif()
endforeach()

file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOSConfig.h"
     "${FREERTOS_WRAPPER_DIR}/FreeRTOSConfig.h" SYMBOLIC)

# ── Path aliases ─────────────────────────────────────────────────────
# test_apps/pc/ → test_apps/ → esp_service/
set(ESP_SERVICE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(ADF_EVENT_HUB_DIR "${ESP_SERVICE_DIR}/../adf_event_hub")
# Mock services live with the usage example so the two stay in sync.
set(EXAMPLE_COMPS_DIR "${ESP_SERVICE_DIR}/examples/mock_services/components")

# ── adf_event_hub library ────────────────────────────────────────────
add_library(adf_event_hub STATIC
    ${ADF_EVENT_HUB_DIR}/adf_event_hub.c
    ${ADF_EVENT_HUB_DIR}/adf_vec.c
)
target_include_directories(adf_event_hub PUBLIC
    ${ADF_EVENT_HUB_DIR}/include
)
target_link_libraries(adf_event_hub PUBLIC freertos_kernel pthread)

# ── esp_service library (core + MCP server, transport-agnostic) ──────
add_library(esp_service STATIC
    ${ESP_SERVICE_DIR}/service/esp_service.c
    ${ESP_SERVICE_DIR}/service/esp_service_manager.c
    ${ESP_SERVICE_DIR}/mcp/esp_service_mcp_server.c
    ${ESP_SERVICE_DIR}/mcp/esp_service_mcp_trans.c
)
target_include_directories(esp_service PUBLIC
    ${ESP_SERVICE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_BINARY_DIR}/freertos_wrapper
)
target_link_libraries(esp_service PUBLIC
    adf_event_hub
    cjson
    freertos_kernel
    pthread
)
target_compile_options(esp_service PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/esp_idf_compat.h"
)

# ── Example services ──────────────────────────────────────────────────
#
#   example_services      — pure service logic; zero MCP code, zero cJSON
#   example_services_mcp  — MCP adapters (tool_invoke + JSON schemas);
#                           compiled separately so Scenario 1 never links them
#
add_library(example_services STATIC
    ${EXAMPLE_COMPS_DIR}/player_service/player_service.c
    ${EXAMPLE_COMPS_DIR}/led_service/led_service.c
    ${EXAMPLE_COMPS_DIR}/ota_service/ota_service.c
    ${EXAMPLE_COMPS_DIR}/wifi_service/wifi_service.c
    ${EXAMPLE_COMPS_DIR}/button_service/button_service.c
    ${EXAMPLE_COMPS_DIR}/cloud_service/cloud_service.c
)
target_include_directories(example_services PUBLIC
    ${EXAMPLE_COMPS_DIR}/player_service
    ${EXAMPLE_COMPS_DIR}/led_service
    ${EXAMPLE_COMPS_DIR}/ota_service
    ${EXAMPLE_COMPS_DIR}/wifi_service
    ${EXAMPLE_COMPS_DIR}/button_service
    ${EXAMPLE_COMPS_DIR}/cloud_service
)
target_link_libraries(example_services PUBLIC esp_service)

add_library(example_services_mcp STATIC
    ${EXAMPLE_COMPS_DIR}/player_service/player_service_mcp.c
    ${EXAMPLE_COMPS_DIR}/led_service/led_service_mcp.c
    ${EXAMPLE_COMPS_DIR}/ota_service/ota_service_mcp.c
    ${EXAMPLE_COMPS_DIR}/wifi_service/wifi_service_mcp.c
    embedded_files.c          # JSON schema blobs referenced by *_mcp.c
)
target_include_directories(example_services_mcp PUBLIC
    ${EXAMPLE_COMPS_DIR}/player_service
    ${EXAMPLE_COMPS_DIR}/led_service
    ${EXAMPLE_COMPS_DIR}/ota_service
    ${EXAMPLE_COMPS_DIR}/wifi_service
)
target_link_libraries(example_services_mcp PUBLIC
    example_services
    cjson
)

# ── Scenario 1: pure function calls — no manager, no MCP ─────────────
#
#   Services created and called directly via their C API.
#   Links example_services only; cjson and MCP code are NOT linked.
#
add_executable(service_sim_example
    service_sim_example.c
    sim_support.c
)
target_include_directories(service_sim_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(service_sim_example PRIVATE
    example_services
    esp_service
)

# ── Scenario 2: Service Manager + tool dispatch — no MCP server ───────
#
#   Services registered with tool_desc + tool_invoke.
#   esp_service_manager_invoke_tool() used for local dispatch by tool name.
#   cjson is needed (tool arg/result parsing) but no MCP transport at all.
#
add_executable(service_tool_dispatch_test
    service_tool_dispatch_test.c
)
target_include_directories(service_tool_dispatch_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(service_tool_dispatch_test PRIVATE
    example_services_mcp
    esp_service
    cjson
)

# ── Scenario 3: long-running MCP HTTP server ─────────────────────────
#
#   Full stack: MCP JSON-RPC over HTTP.  A remote MCP client (e.g. Claude,
#   or the tools/mcp_llm_bridge companion) can discover tools and invoke
#   them over the network.  This is a runtime server, not a self-contained
#   test — it runs until SIGINT/SIGTERM.
#
add_executable(mcp_http_server
    mcp_http_server.c
    host_http_transport.c
)
target_include_directories(mcp_http_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(mcp_http_server PRIVATE
    example_services_mcp
    esp_service
    cjson
)

# ── Coverage Test: exhaustive boundary / stress / MT suite ────────────
#
#   Unity-style comprehensive test exercising adf_event_hub,
#   esp_service and esp_service_manager APIs across NULL/boundary
#   arguments, full state-machine transitions, high-volume multi-threaded
#   publish/subscribe and concurrent invoke_tool dispatch.
#
add_executable(service_coverage_test
    service_coverage_test.c
)
target_include_directories(service_coverage_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(service_coverage_test PRIVATE
    example_services_mcp
    esp_service
    cjson
)

# ── Integration Test: all three interaction modes coexisting ──────────
#
#   Runs Mode A (direct C API), Mode B (tool dispatch), and Mode C (MCP HTTP)
#   concurrently against the same service instances for ~12 seconds,
#   then verifies the results.
#
add_executable(service_integration_test
    service_integration_test.c
    host_http_transport.c
)
target_include_directories(service_integration_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(service_integration_test PRIVATE
    example_services_mcp
    esp_service
    cjson
)

# Enable _GNU_SOURCE for asprintf on Linux
if(NOT APPLE)
    target_compile_definitions(service_sim_example         PRIVATE _GNU_SOURCE)
    target_compile_definitions(service_tool_dispatch_test  PRIVATE _GNU_SOURCE)
    target_compile_definitions(mcp_http_server             PRIVATE _GNU_SOURCE)
    target_compile_definitions(service_integration_test    PRIVATE _GNU_SOURCE)
    target_compile_definitions(service_coverage_test       PRIVATE _GNU_SOURCE)
    target_compile_definitions(esp_service                 PRIVATE _GNU_SOURCE)
endif()
