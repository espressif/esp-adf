include("${CMAKE_CURRENT_LIST_DIR}/../../common/esp_bt_hub_test_common.cmake")

idf_component_register(SRCS
                            ${ESP_BT_HUB_TEST_COMMON_SRCS}
                       INCLUDE_DIRS
                            ${ESP_BT_HUB_TEST_COMMON_INCLUDE_DIRS}
                       PRIV_INCLUDE_DIRS
                            "../../../private_inc"
                       REQUIRES
                            bt
                            unity
                            esp_bt_hub
                            nvs_flash
                       WHOLE_ARCHIVE)
