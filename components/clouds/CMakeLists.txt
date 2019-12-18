set(COMPONENT_ADD_INCLUDEDIRS ./dueros/lightduer/include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_SRCS )


register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${CMAKE_CURRENT_LIST_DIR}/dueros/lightduer")
target_link_libraries(${COMPONENT_TARGET} INTERFACE duer-device)

