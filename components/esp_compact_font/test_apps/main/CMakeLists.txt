idf_component_register(
    SRCS
        "app_main.c"
        "test_cache.c"
        "test_decode_draw.c"
        "test_file_vfs.c"
        "test_codecs_match.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        unity
        esp_system
        esp_compact_font
        vfs
    WHOLE_ARCHIVE
)

set(_assets "${CMAKE_CURRENT_LIST_DIR}/../assets")
foreach(_bin ascii_16_2bpp.gfh ascii_16_2bpp.gfz ascii_16_2bpp.bin)
    set(_path "${_assets}/${_bin}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Missing test font ${_path}")
    endif()
    target_add_binary_data(${COMPONENT_LIB} "${_path}" BINARY)
endforeach()
