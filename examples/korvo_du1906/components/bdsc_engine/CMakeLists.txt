set(COMPONENT_ADD_INCLUDEDIRS include)

set(COMPONENT_REQUIRES mqtt json)

register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L ${CMAKE_CURRENT_SOURCE_DIR}/lib")

add_prebuilt_library(bdsc_engine "${CMAKE_CURRENT_SOURCE_DIR}/lib/libbdsc_engine.a" PRIV_REQUIRES json mqtt audio_flash_tone)

target_link_libraries(${COMPONENT_TARGET} INTERFACE bdsc_engine)
