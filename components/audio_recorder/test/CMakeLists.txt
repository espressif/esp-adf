set(COMPONENT_ADD_INCLUDEDIRS .)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES audio_sal esp-sr audio_pipeline test_utils audio_recorder)

set(COMPONENT_SRCS test_audio_recorder.c)

set(COMPONENT_EMBED_FILES 4ch16bit16k.pcm 2ch16bit16k.pcm 1ch16bit16k.pcm)

register_component()