execute_process(COMMAND git describe --always --tags --dirty
                OUTPUT_VARIABLE ADF_VER
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
add_definitions(-DADF_VER=\"${ADF_VER}\")

if(NOT DEFINED ENV{IDF_PATH})
set(ENV{IDF_PATH} "$ENV{ADF_PATH}/esp-idf/")
endif()
set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} $ENV{ADF_PATH}/components)
