cmake_minimum_required(VERSION 3.16)

set(SUPPORTED_TARGETS esp32s3 esp32s31 esp32p4)

if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_llm_baidu_test_app)
