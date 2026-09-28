cmake_minimum_required(VERSION 3.16)

message(STATUS "................. ${CMAKE_SYSTEM_NAME}")
if("${CMAKE_SYSTEM_NAME}" MATCHES "(Rtos|rtos|RTOS)+")
  set(PLATFORM_RTOS TRUE)
  add_compile_definitions(PLATFORM_RTOS)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "(Linux|linux|LINUX)+")
  set(PLATFORM_LINUX TRUE)
  add_compile_definitions(PLATFORM_LINUX)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "(android|Android|ANDROID)+")
  set(PLATFORM_ANDROID TRUE)
  add_compile_definitions(PLATFORM_ANDROID)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "(Darwin|darwin|MACOSX|macosx|MACOS|macos)+")
  set(PLATFORM_MACOS TRUE)
  add_compile_definitions(PLATFORM_MACOS)
  set(VOLC_CONV_AI_PLATFORM_SRCS "osal/src/macos/volc_osal.c"
                        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 platform src file")
  message(STATUS "VOLC_CONV_AI_PLATFORM_SRCS: ${VOLC_CONV_AI_PLATFORM_SRCS}")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)
if(ESP_PLATFORM)
    message(STATUS "ESP_PLATFORM is defined, including esp.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/espressif.cmake)
    return()
elseif(PLATFORM_LINUX)
    message(STATUS "PLATFORM_LINUX is defined, including linux.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/linux.cmake)
elseif(PLATFORM_MACOS)
    message(STATUS "PLATFORM_MACOS is defined, including macos.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/macos.cmake)
    return()
endif()

if(PLATFORM_LINUX AND ENABLE_SAMPLE)
  message(STATUS "Linux平台的test_sample")
  add_executable(
        test_sample
        sample/sample.c
  )
  target_include_directories(
    test_sample
    PRIVATE
    ${VOLC_CONV_AI_INCS}
  )
  target_link_libraries(test_sample PRIVATE
    volc_conv_ai_a
  )
endif()
