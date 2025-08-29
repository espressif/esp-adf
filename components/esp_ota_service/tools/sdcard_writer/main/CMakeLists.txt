get_filename_component(FIRMWARE_SAMPLES_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware_samples" ABSOLUTE)

# sdcard_writer embeds three optional payloads and writes them to the SD card.
# Input paths are chosen in this order:
#   1. -D<VAR>=... CMake cache var or a matching environment variable.
#   2. Newest file matching firmware_samples/<prefix>_v*.bin (semver sorted).
# The wrapper script tools/ota_fs_flash_sdcard.py always passes absolute
# paths via -D so CI / IDF builds stay deterministic.
#
# Variables (all optional except OTA_FS_BIN which must resolve to something):
#   OTA_FS_BIN          -> SD target /sdcard/app.bin        (source: ota_fs_v*.bin)
#   OTA_FS_USERDATA_BIN -> SD target /sdcard/data.bin       (source: userdata_v*.bin)
#   OTA_FS_TONE_BIN     -> SD target /sdcard/flash_tone.bin (source: flash_tone_v*.bin)

function(_sdw_latest_versioned out_var prefix)
    file(GLOB _matches "${FIRMWARE_SAMPLES_DIR}/${prefix}_v*.bin")
    if(NOT _matches)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    # Sort lexicographically by version tokens padded to 3 digits so 1.10.0
    # beats 1.2.0 even in purely string-based sort.
    set(_indexed "")
    foreach(_p IN LISTS _matches)
        # NAME_WE would strip from the first '.' so ota_fs_v0.2.0.bin becomes
        # ota_fs_v0 - use the full filename and anchor to the .bin suffix.
        if(_p MATCHES "_v([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.bin$")
            set(_maj "${CMAKE_MATCH_1}")
            set(_min "${CMAKE_MATCH_2}")
            set(_pat "${CMAKE_MATCH_3}")
            # Zero-pad each numeric token to 3 chars so pure string sort
            # orders 1.10.0 after 1.2.0 correctly.
            string(LENGTH "${_maj}" _lmaj)
            math(EXPR _pmaj "3 - ${_lmaj}")
            string(LENGTH "${_min}" _lmin)
            math(EXPR _pmin "3 - ${_lmin}")
            string(LENGTH "${_pat}" _lpat)
            math(EXPR _ppat "3 - ${_lpat}")
            set(_zmaj "${_maj}")
            set(_zmin "${_min}")
            set(_zpat "${_pat}")
            if(_pmaj GREATER 0)
                string(REPEAT "0" ${_pmaj} _z0)
                set(_zmaj "${_z0}${_maj}")
            endif()
            if(_pmin GREATER 0)
                string(REPEAT "0" ${_pmin} _z0)
                set(_zmin "${_z0}${_min}")
            endif()
            if(_ppat GREATER 0)
                string(REPEAT "0" ${_ppat} _z0)
                set(_zpat "${_z0}${_pat}")
            endif()
            list(APPEND _indexed "${_zmaj}.${_zmin}.${_zpat}|${_p}")
        endif()
    endforeach()
    if(NOT _indexed)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    list(SORT _indexed)
    list(REVERSE _indexed)
    list(GET _indexed 0 _top)
    string(REGEX REPLACE "^[^|]+\\|" "" _top_path "${_top}")
    set(${out_var} "${_top_path}" PARENT_SCOPE)
endfunction()

function(_sdw_resolve var_name prefix)
    set(_raw "${${var_name}}")
    if(NOT _raw AND DEFINED ENV{${var_name}})
        set(_raw "$ENV{${var_name}}")
    endif()
    if(_raw)
        set(${var_name} "${_raw}" PARENT_SCOPE)
        return()
    endif()
    _sdw_latest_versioned(_auto "${prefix}")
    set(${var_name} "${_auto}" PARENT_SCOPE)
endfunction()

_sdw_resolve(OTA_FS_BIN          "ota_fs")
_sdw_resolve(OTA_FS_USERDATA_BIN "userdata")
_sdw_resolve(OTA_FS_TONE_BIN     "flash_tone")

if(NOT OTA_FS_BIN OR NOT EXISTS "${OTA_FS_BIN}")
    message(FATAL_ERROR
        "sdcard_writer: no ota_fs firmware bin found.\n"
        "  Expected: -DOTA_FS_BIN=<path> or firmware_samples/ota_fs_v*.bin\n"
        "  Publish one first, e.g.:\n"
        "      tools/build_firmware.py fs -v 0.2.0")
endif()

# Stage the inputs into the build tree under stable filenames so the asm
# symbols exported by EMBED_FILES stay independent of the source version.
set(_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/sdw_payload")
file(MAKE_DIRECTORY "${_stage_dir}")

set(_staged_app      "${_stage_dir}/app.bin")
set(_staged_userdata "${_stage_dir}/userdata.bin")
set(_staged_tone     "${_stage_dir}/flash_tone.bin")

configure_file("${OTA_FS_BIN}" "${_staged_app}" COPYONLY)
message(STATUS "sdcard_writer: app.bin <- ${OTA_FS_BIN}")

set(_embed_files "${_staged_app}")
set(_defs "")

if(OTA_FS_USERDATA_BIN AND EXISTS "${OTA_FS_USERDATA_BIN}")
    configure_file("${OTA_FS_USERDATA_BIN}" "${_staged_userdata}" COPYONLY)
    list(APPEND _embed_files "${_staged_userdata}")
    list(APPEND _defs "SDW_HAS_USERDATA=1")
    message(STATUS "sdcard_writer: userdata.bin <- ${OTA_FS_USERDATA_BIN}")
else()
    message(STATUS "sdcard_writer: userdata.bin skipped")
endif()

if(OTA_FS_TONE_BIN AND EXISTS "${OTA_FS_TONE_BIN}")
    configure_file("${OTA_FS_TONE_BIN}" "${_staged_tone}" COPYONLY)
    list(APPEND _embed_files "${_staged_tone}")
    list(APPEND _defs "SDW_HAS_FLASH_TONE=1")
    message(STATUS "sdcard_writer: flash_tone.bin <- ${OTA_FS_TONE_BIN}")
else()
    message(STATUS "sdcard_writer: flash_tone.bin skipped")
endif()

idf_component_register(
    SRCS         "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES     fatfs
    EMBED_FILES  ${_embed_files}
)

if(_defs)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ${_defs})
endif()
