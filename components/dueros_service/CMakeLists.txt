set(COMPONENT_ADD_INCLUDEDIRS include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES clouds mbedtls audio_board audio_hal esp-adf-libs audio_sal esp_peripherals audio_recorder wifi_service)

set (duer_srcs
    "dueros_service.c"
    "esp_audio_device_info.c"
    "dueros_esp_flash.c"
    "duer_profile.c"
    "duer_wifi_cfg.c"
)

if (CONFIG_BT_BLE_ENABLED)
    list(APPEND duer_srcs "duer_wifi_cfg_bluedroid.c")
else()
    list(APPEND duer_srcs "duer_wifi_cfg_dummy.c")
endif()

set(COMPONENT_SRCS ${duer_srcs})

register_component()

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u duer_flash_erase")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u duer_flash_read")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u duer_flash_write")

partition_table_get_partition_info(NVS_PARTITION_SIZE "--partition-name nvs" "size")

if ("${NVS_PARTITION_SIZE}" AND CONFIG_DUEROS_GEN_PROFILE)

    set(NVS_GEN_PY $ENV{IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py)
    set(NVS_IMAGE  ${CMAKE_BINARY_DIR}/duer_profile.bin)
    set(NVS_TABLE  ${COMPONENT_DIR}/nvs.csv)

    add_custom_target(duer_profile_bin ALL
        COMMAND ${PYTHON} ${NVS_GEN_PY} generate ${NVS_TABLE} ${NVS_IMAGE} ${NVS_PARTITION_SIZE}
        WORKING_DIRECTORY ${COMPONENT_DIR}
        DEPENDS ${COMPONENT_DIR}/duer_profile ${NVS_TABLE}
    )

    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
            ADDITIONAL_MAKE_CLEAN_FILES
            ${NVS_IMAGE})

    if (CONFIG_DUEROS_FLASH_PROFILE)
        add_dependencies(flash duer_profile_bin)
        esptool_py_flash_to_partition(flash "nvs" "${NVS_IMAGE}")
    endif()
endif()
