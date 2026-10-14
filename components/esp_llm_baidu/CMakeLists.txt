idf_build_get_property(idf_target IDF_TARGET)
set(ESP_LLM_BAIDU_SUPPORTED_TARGETS esp32s3 esp32s31 esp32p4)
if(NOT idf_target IN_LIST ESP_LLM_BAIDU_SUPPORTED_TARGETS)
    message(FATAL_ERROR
            "esp_llm_baidu only supports esp32s3, esp32s31, and esp32p4 (got ${idf_target})")
endif()

idf_component_register(
    SRCS
        "src/esp_llm_baidu_adapter.c"
        "src/esp_llm_baidu_agent.c"
        "src/esp_llm_baidu_intercom.c"
        "src/esp_llm_baidu_profile.c"
        "src/esp_llm_baidu_libc_compat.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "components/brtc/include"
    # esp_llm_service is declared in idf_component.yml.
    PRIV_REQUIRES lwip mbedtls
)

idf_component_get_property(esp_llm_service_dir esp_llm_service COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_llm_service_dir}/private_include")

# Vendored Baidu RTC prebuilt. Not a separate component.
set(baidu_rtc_requires mbedtls espressif__zlib lwip esp_wifi esp_netif)
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND baidu_rtc_requires esp_libc)
else()
    list(APPEND baidu_rtc_requires newlib)
endif()
add_prebuilt_library(libbrtc
    "${CMAKE_CURRENT_SOURCE_DIR}/components/brtc/libs/${CONFIG_IDF_TARGET}/libbrtc.a"
    REQUIRES ${baidu_rtc_requires})
target_link_libraries(${COMPONENT_LIB} INTERFACE libbrtc)
idf_component_optional_requires(PRIVATE json espressif__cjson)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _ctype_")
