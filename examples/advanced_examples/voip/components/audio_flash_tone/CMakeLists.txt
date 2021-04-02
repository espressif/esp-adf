set(COMPONENT_SRCS "audio_tone_uri.c" "audio_player_int_tone.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

set(COMPONENT_REQUIRES esp-adf-libs audio_stream)

register_component()