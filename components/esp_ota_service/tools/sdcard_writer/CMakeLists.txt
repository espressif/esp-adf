cmake_minimum_required(VERSION 3.16)
# Standalone IDF project (no ADF_PATH) — only writes embedded firmware to SD; avoids
# pulling esp-adf-libs / audio_pipeline when ADF components are incomplete in this tree.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sdcard_writer)
