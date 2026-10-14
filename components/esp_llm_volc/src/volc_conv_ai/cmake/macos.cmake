message(STATUS "配置macOS平台的volc_conv_ai库")

option(ENABLE_RTC_MODE "Enable Conv AI RTC mode" OFF)
option(ENABLE_WS_MODE  "Enable Conv AI WS mode"  OFF)
option(ENABLE_CJSON "Enable cJSON" ON)
option(ENABLE_MBEDTLS "Enable Mbedtls" ON)

if(NOT DEFINED VOLC_CONV_AI_PLATFORM_SRCS)
    set(VOLC_CONV_AI_PLATFORM_SRCS
        "${CMAKE_CURRENT_LIST_DIR}/../osal/src/macos/volc_osal.c"
        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 platform src file")
endif()

set(MBEDTLS_PREBUILT_DIR ${CMAKE_CURRENT_LIST_DIR}/../third_party/prebuilt/mbedtls)
set(MBEDTLS_LIBRARY_FILE "${MBEDTLS_PREBUILT_DIR}/lib/libmbedtls.a")
set(MBEDTLS_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls)

if(NOT EXISTS "${MBEDTLS_LIBRARY_FILE}" AND NOT EXISTS "${MBEDTLS_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "start download and compile mbedtls...")
    include(ExternalProject)
    ExternalProject_Add(
      mbedtls
      GIT_REPOSITORY  https://github.com/ARMmbed/mbedtls.git
      GIT_TAG         v3.6.3
      PREFIX          ${CMAKE_CURRENT_BINARY_DIR}/build
      SOURCE_DIR      ${MBEDTLS_SOURCE_DIR}
      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${MBEDTLS_PREBUILT_DIR}
        -DUSE_SHARED_MBEDTLS_LIBRARY=${BUILD_SHARED}
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_MACOSX_RPATH=${CMAKE_MACOSX_RPATH}
        -DENABLE_TESTING=OFF
        -DENABLE_PROGRAMS=OFF
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} -Wno-unused-but-set-parameter
        -DMBEDTLS_FATAL_WARNINGS=OFF

      BUILD_ALWAYS    FALSE
      TEST_COMMAND    ""
    )
else()
    message(STATUS "mbedtls library already exists, skip download and compile")
endif()

add_library(volc_conv_ai_a STATIC
    ${VOLC_CONV_AI_SRCS}
    ${VOLC_CONV_AI_PLATFORM_SRCS}
)

if(ENABLE_WS_MODE)
    message(STATUS "use low load WS mode")
    target_sources(volc_conv_ai_a PRIVATE
        ${VOLC_CONV_AI_LOW_LOAD_SRCS}
    )
endif()

add_library(mbedtls_static   STATIC IMPORTED GLOBAL)
add_library(mbedx509_static  STATIC IMPORTED GLOBAL)
add_library(zlib_static STATIC IMPORTED GLOBAL)
add_library(mbedcrypto_static STATIC IMPORTED GLOBAL)

set_target_properties(zlib_static PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/../third_party/prebuilt/zlib/lib/libz.a
)
set_target_properties(mbedtls_static PROPERTIES
    IMPORTED_LOCATION ${MBEDTLS_PREBUILT_DIR}/lib/libmbedtls.a
)
set_target_properties(mbedx509_static PROPERTIES
    IMPORTED_LOCATION ${MBEDTLS_PREBUILT_DIR}/lib/libmbedx509.a
)
set_target_properties(mbedcrypto_static PROPERTIES
    IMPORTED_LOCATION ${MBEDTLS_PREBUILT_DIR}/lib/libmbedcrypto.a
)

if(ENABLE_RTC_MODE)
    target_sources(volc_conv_ai_a PRIVATE
        ${VOLC_CONV_AI_HIGH_QUALITY_SRCS}
    )
    message(STATUS "use high quality RTC mode")
    set(RTC_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../src/transports/high_quality/third_party/volc_rtc_engine_lite/libs/macos)
    message(STATUS "RTC_LIB_DIR: ${RTC_LIB_DIR}")
    find_library(RTC_LIBRARIES NAMES VolcEngineRTCLite REQUIRED NO_CMAKE_FIND_ROOT_PATH PATHS ${RTC_LIB_DIR})
    
    target_link_libraries(volc_conv_ai_a PRIVATE
        ${RTC_LIBRARIES}
    )
endif()

if(ENABLE_WS_MODE)
    target_compile_definitions(volc_conv_ai_a PRIVATE ENABLE_WS_MODE)
endif()

if(ENABLE_RTC_MODE)
    target_compile_definitions(volc_conv_ai_a PRIVATE ENABLE_RTC_MODE)
endif()

if(ENABLE_CJSON)
    target_compile_definitions(volc_conv_ai_a PRIVATE ENABLE_CJSON)
endif()

if(ENABLE_MBEDTLS)
    target_include_directories(volc_conv_ai_a PUBLIC ${MBEDTLS_PREBUILT_DIR}/include)
    
    target_link_libraries(volc_conv_ai_a PRIVATE
        mbedtls_static
        mbedx509_static
        mbedcrypto_static
        zlib_static
    )
    
    target_compile_definitions(volc_conv_ai_a PRIVATE ENABLE_MBEDTLS)
endif()

target_include_directories(volc_conv_ai_a PUBLIC
    ${VOLC_CONV_AI_INCS}
    ${VOLC_CONV_AI_PLATFORM_INCS}
)

if(ENABLE_WS_MODE)
    target_include_directories(volc_conv_ai_a PUBLIC
        ${VOLC_CONV_AI_LOW_LOAD_INCS}
    )
endif()

if(ENABLE_RTC_MODE)
    target_include_directories(volc_conv_ai_a PUBLIC
        ${VOLC_CONV_AI_HIGH_QUALITY_INCS}
    )
endif()

set_target_properties(volc_conv_ai_a PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
