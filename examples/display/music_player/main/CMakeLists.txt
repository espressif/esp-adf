IF (((IDF_VERSION_MAJOR EQUAL 4) AND (IDF_VERSION_MINOR GREATER 3)) OR (IDF_VERSION_MAJOR GREATER 4))
file(GLOB_RECURSE DEMO_SOURCES *.c)
ELSE()
set(DEMO_SOURCES "")
ENDIF()
idf_component_register(
    SRCS
        ${DEMO_SOURCES} lv_demos.c
    INCLUDE_DIRS
        ".")

# Some of warinings from LVGL. Block them.
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
