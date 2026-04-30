
file(GLOB SHARED_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../common/*.c")

idf_component_register(
    SRCS "app_main.c"
         ${SHARED_SOURCES}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}"
                 "${CMAKE_CURRENT_LIST_DIR}/../common"
    REQUIRES
        adf_event_hub
)
