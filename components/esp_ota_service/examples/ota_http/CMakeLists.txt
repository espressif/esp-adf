cmake_minimum_required(VERSION 3.16)

get_filename_component(_ADF_COMPONENTS "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${_ADF_COMPONENTS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ota_http)

# Publishing to tools/firmware_samples/ is done by tools/build_firmware.py
# (writes a versioned ota_http_v<ver>.bin and refreshes manifest.json). Run it
# manually after a version bump (from components/esp_ota_service/tools/):
#   ./build_firmware.py http -v 1.0.1
