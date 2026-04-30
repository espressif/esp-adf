#
# Benchmarks are built alongside the unit-test executable but registered as
# a separate CTest target gated by CONFIGURATIONS=Bench, so a default
# `ctest` run skips them.  Run explicitly with:
#     ctest -C Bench

add_executable(bench_event_hub
    bench_event_hub.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.c)
target_include_directories(bench_event_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
target_link_libraries(bench_event_hub PRIVATE adf_event_hub_pc unity_lib)

if(ADF_EH_SANITIZER)
    set(_sanitize_flags -fsanitize=${ADF_EH_SANITIZER} -fno-omit-frame-pointer -g)
    target_compile_options(bench_event_hub PRIVATE ${_sanitize_flags})
    target_link_options(bench_event_hub    PRIVATE -fsanitize=${ADF_EH_SANITIZER})
endif()

add_test(NAME adf_event_hub_bench COMMAND bench_event_hub)
set_tests_properties(adf_event_hub_bench PROPERTIES
    CONFIGURATIONS Bench
    TIMEOUT        600)
