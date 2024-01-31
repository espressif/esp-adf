set(COMPONENT_SRCS "src_drv_adc.c" "src_drv_i2s.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES audio_sal audio_board driver audio_stream)

register_component()
