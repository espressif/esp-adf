#
# The ESP-IDF test firmware uses the same cases on hardware and in EMU.

file(GLOB SHARED_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../common/test_*.c")

idf_component_register(
    SRCS "app_main.c"
         ${SHARED_TEST_SOURCES}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}"
                 "${CMAKE_CURRENT_LIST_DIR}/../common"
    PRIV_REQUIRES unity test_utils adf_event_hub
    WHOLE_ARCHIVE
)
