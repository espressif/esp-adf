if (DEFINED ENV{DUER_PATH})
    include($ENV{DUER_PATH}/libduer.cmake)
    return()
endif()

set(COMPONENT_ADD_INCLUDEDIRS ./dueros/lightduer/include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_SRCS )


register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${CMAKE_CURRENT_LIST_DIR}/dueros/lightduer/${CONFIG_IDF_TARGET}")

IF ((CONFIG_IDF_TARGET STREQUAL "esp32") OR (CONFIG_IDF_TARGET STREQUAL "esp32s3") OR (CONFIG_IDF_TARGET STREQUAL "esp32p4"))
IF (IDF_VERSION_MAJOR GREATER 4)
target_link_libraries(${COMPONENT_TARGET} INTERFACE duer-device-v5x)
ELSEIF (IDF_VERSION_MAJOR GREATER 3)
target_link_libraries(${COMPONENT_TARGET} INTERFACE duer-device-v4x)
ELSE ()
target_link_libraries(${COMPONENT_TARGET} INTERFACE duer-device)
ENDIF (IDF_VERSION_MAJOR GREATER 4)
ELSE()
ENDIF()
