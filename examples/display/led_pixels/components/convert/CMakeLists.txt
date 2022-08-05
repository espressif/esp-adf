idf_component_register(SRCS . "cnv.c" "cnv_pattern.c"
                    INCLUDE_DIRS "include" "cnv_audio" "cnv_basic"
                    REQUIRES driver esp-dsp source_drv audio_sal utilis pixel_renderer)

message(STATUS "Chip is ${CONFIG_IDF_TARGET}, ${CMAKE_CURRENT_SOURCE_DIR}")

add_prebuilt_library(convert "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CONFIG_IDF_TARGET}/libconvert.a"
                        REQUIRES driver source_drv audio_sal utilis pixel_renderer)

target_link_libraries(${COMPONENT_TARGET} INTERFACE convert)
