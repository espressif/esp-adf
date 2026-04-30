#
# PC build of the adf_event_hub multi-service example.  Reuses the shared
# service sources under ../common/ that also drive the IDF build under
# ../main/ — only the entry point differs.  FreeRTOS-Kernel (POSIX port) is
# fetched automatically.

cmake_minimum_required(VERSION 3.16)
project(adf_event_hub_example_pc C)

include(FetchContent)

# ── FreeRTOS Kernel (POSIX/GCC simulator) ─────────────────────────────
set(FREERTOS_HEAP                  "3"         CACHE STRING "" FORCE)
set(FREERTOS_PORT                  "GCC_POSIX" CACHE STRING "" FORCE)
set(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "" FORCE)

FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(freertos_kernel)

if(APPLE)
    target_compile_options(freertos_kernel PRIVATE -Wno-deprecated-declarations)
endif()

# ── adf_event_hub PC library (component under test) ─────────────────
add_library(adf_event_hub_pc STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../adf_event_hub.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../adf_vec.c)
target_include_directories(adf_event_hub_pc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(adf_event_hub_pc PUBLIC freertos_kernel pthread)

# ── Shared service sources (also consumed by the IDF build) ─────────
set(EXAMPLE_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
file(GLOB EXAMPLE_COMMON_SOURCES ${EXAMPLE_COMMON_DIR}/*.c)

add_executable(event_hub_example
    main.c
    ${EXAMPLE_COMMON_SOURCES})
target_include_directories(event_hub_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXAMPLE_COMMON_DIR})
target_link_libraries(event_hub_example PRIVATE adf_event_hub_pc)
