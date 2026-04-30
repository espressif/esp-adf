cmake_minimum_required(VERSION 3.16)

# services_hub -> adf_examples -> repo root -> components
get_filename_component(_COMPONENTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../components" ABSOLUTE)

# Keep dependencies explicit to avoid pulling unrelated components/manifests.
set(EXTRA_COMPONENT_DIRS
    "${_COMPONENTS_DIR}"
    "${_COMPONENTS_DIR}/esp_service/examples/mock_services/components/wifi_service"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(services_hub)
