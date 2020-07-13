set(COMPONENT_ADD_INCLUDEDIRS .
                            include)

register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L ${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_link_libraries(${COMPONENT_TARGET} INTERFACE bds_private_info)

