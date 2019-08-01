set(COMPONENT_SRCS "esp_player_wrapper.c")
set(COMPONENT_ADD_INCLUDEDIRS .)


set(COMPONENT_REQUIRES audio_sal esp-adf-libs audio_pipeline)

register_component()
