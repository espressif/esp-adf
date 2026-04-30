#
# PC build of the adf_event_hub unit tests.  Consumes the shared test bodies
# under ../common/ that also drive the IDF target build in ../main/.  Fetches
# FreeRTOS-Kernel (POSIX port) + Unity so the harness is self-contained.
#
# Options:
#   -DADF_EH_SANITIZER=address|undefined|thread   Enable a compiler sanitizer.
#   -DADF_EH_COVERAGE=ON                          Instrument for gcov/lcov.
# Only one of the two is expected to be enabled at a time.

cmake_minimum_required(VERSION 3.16)
project(adf_event_hub_pc_tests C)

option(ADF_EH_COVERAGE  "Instrument PC build for gcov/lcov"                OFF)
set(ADF_EH_SANITIZER "" CACHE STRING "Enable compiler sanitizer (address|undefined|thread), empty to disable")

include(FetchContent)

# ── FreeRTOS Kernel (POSIX/GCC simulator) ─────────────────────────────
set(FREERTOS_HEAP                  "3"         CACHE STRING "" FORCE)
set(FREERTOS_PORT                  "GCC_POSIX" CACHE STRING "" FORCE)
set(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "" FORCE)

FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE)

FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(freertos_kernel unity)

if(APPLE)
    target_compile_options(freertos_kernel PRIVATE -Wno-deprecated-declarations)
endif()

# ── Unity library ────────────────────────────────────────────────────
add_library(unity_lib STATIC ${unity_SOURCE_DIR}/src/unity.c)
target_include_directories(unity_lib PUBLIC ${unity_SOURCE_DIR}/src)

# ── adf_event_hub PC library (component under test) ─────────────────
add_library(adf_event_hub_pc STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../adf_event_hub.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../adf_vec.c)
target_include_directories(adf_event_hub_pc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(adf_event_hub_pc PUBLIC freertos_kernel pthread)

# ── Shared test sources (also consumed by the IDF target build) ─────
file(GLOB SHARED_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_*.c)

# ── PC test executable ──────────────────────────────────────────────
add_executable(test_event_hub
    main.c
    ${SHARED_TEST_SOURCES})
target_include_directories(test_event_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(test_event_hub PRIVATE adf_event_hub_pc unity_lib)

# ── Optional sanitizer toggle ───────────────────────────────────────
if(ADF_EH_SANITIZER)
    set(_sanitize_flags -fsanitize=${ADF_EH_SANITIZER} -fno-omit-frame-pointer -g)
    target_compile_options(adf_event_hub_pc PRIVATE ${_sanitize_flags})
    target_compile_options(test_event_hub   PRIVATE ${_sanitize_flags})
    target_link_options(test_event_hub      PRIVATE -fsanitize=${ADF_EH_SANITIZER})
endif()

# ── Optional coverage toggle (PC-only; IDF build is never instrumented) ─
if(ADF_EH_COVERAGE)
    set(_cov_flags --coverage -O0 -g -fno-inline)
    target_compile_options(adf_event_hub_pc PRIVATE ${_cov_flags})
    target_compile_options(test_event_hub   PRIVATE ${_cov_flags})
    target_link_options(adf_event_hub_pc    PUBLIC  --coverage)
    target_link_options(test_event_hub      PRIVATE --coverage)
endif()

# ── Separate benchmark executable (not run by default) ──────────────
add_subdirectory(bench)

# ── CTest integration ───────────────────────────────────────────────
enable_testing()
add_test(NAME adf_event_hub_pc COMMAND test_event_hub)
set_tests_properties(adf_event_hub_pc PROPERTIES TIMEOUT 120)
