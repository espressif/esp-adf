
idf_component_register(
    INCLUDE_DIRS "include"
)

add_prebuilt_library(libbrtc "${CMAKE_CURRENT_SOURCE_DIR}/libs/${CONFIG_IDF_TARGET}/libbrtc.a"
                    REQUIRES mbedtls espressif__zlib json lwip)
target_link_libraries(${COMPONENT_LIB} INTERFACE libbrtc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wno-unused-variable -Wfatal-errors -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-format -Wno-implicit-function-declaration -Wno-pointer-sign -Wno-double-promotion  -Wno-unused-value -Wno-switch -Wno-return-type -g -O0")  