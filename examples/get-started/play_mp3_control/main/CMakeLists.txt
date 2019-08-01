set(COMPONENT_ADD_INCLUDEDIRS include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS ./play_mp3_control_example.c)
set(COMPONENT_EMBED_TXTFILES adf_music.mp3)
register_component()
