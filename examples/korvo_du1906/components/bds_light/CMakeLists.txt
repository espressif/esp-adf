set(COMPONENT_ADD_INCLUDEDIRS .
                            include)
set(COMPONENT_SRCDIRS  .)
set(COMPONENT_REQUIRES mbedtls freertos esp-tls)

register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L ${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_link_libraries(${COMPONENT_TARGET} INTERFACE bds_light_sdk)
