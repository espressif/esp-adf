#
# IDF component registration for the adf_event_hub unit-test app.  The shared
# test bodies live in ../common/ and are reused by the PC build under ../pc/
# via file(GLOB).

file(GLOB SHARED_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../common/test_*.c")

idf_component_register(
    SRCS "app_main.c"
         ${SHARED_TEST_SOURCES}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}"
                 "${CMAKE_CURRENT_LIST_DIR}/../common"
    PRIV_REQUIRES unity test_utils adf_event_hub
    WHOLE_ARCHIVE
)
