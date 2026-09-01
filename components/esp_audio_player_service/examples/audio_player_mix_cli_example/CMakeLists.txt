cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
)

set(_BMGR_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${_BMGR_DEFAULTS}")
    list(PREPEND SDKCONFIG_DEFAULTS "${_BMGR_DEFAULTS}")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../.."
)

set(_BMGR_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/components/gen_bmgr_codes")
if(EXISTS "${_BMGR_COMPONENT}/CMakeLists.txt")
    list(APPEND EXTRA_COMPONENT_DIRS "${_BMGR_COMPONENT}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(audio_player_mix_cli_example)
