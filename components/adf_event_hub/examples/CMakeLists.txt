cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../adf_event_hub")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(adf_event_hub_example)
