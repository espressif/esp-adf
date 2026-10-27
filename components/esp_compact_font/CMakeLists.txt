idf_component_register(
    SRCS
        "src/esp_cf.c"
        "src/esp_cf_cache.c"
        "src/esp_cf_gfh.c"
        "src/gfh_dec.c"
        "src/esp_cf_scale.c"
        "src/esp_cf_scale_pie.S"
        "src/esp_cf_blend.c"
        "src/esp_cf_blend_pie.S"
        "src/esp_cf_draw.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "private_inc"
        "src"
    LDFRAGMENTS
        "linker.lf"
    REQUIRES
        esp_hw_support
        freertos
        log
    WHOLE_ARCHIVE
)

# GFH is always linked. GFZ, and its zstd decoder, stay out until the
# Kconfig option is turned on.
if(CONFIG_ESP_COMPACT_FONT_GFZ)
    target_sources(${COMPONENT_LIB} PRIVATE
        "src/esp_cf_gfz.c"
        "src/zstd_dec.c")
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/src/zstd_dec.c"
        PROPERTIES COMPILE_DEFINITIONS "ZSTD_DEC_LIT_CAP=4096")
endif()

# LVGL bin is the same kind of codec. The example turns this on. A reader
# build leaves it off unless that firmware should open .bin fonts.
if(CONFIG_ESP_COMPACT_FONT_LVGL)
    target_sources(${COMPONENT_LIB} PRIVATE "src/esp_cf_lvgl.c")
endif()
