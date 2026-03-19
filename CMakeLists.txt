execute_process(COMMAND git describe --always --tags --dirty
                OUTPUT_VARIABLE ADF_VER
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
add_definitions(-DADF_VER=\"${ADF_VER}\")

set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} ${CMAKE_CURRENT_LIST_DIR}/components)
