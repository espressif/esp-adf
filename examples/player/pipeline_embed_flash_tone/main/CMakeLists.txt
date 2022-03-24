include( ${PROJECT_DIR}/main/embed_tone.cmake )
set(COMPONENT_SRCS ./play_embed_flash_example.c)
set(COMPONENT_ADD_INCLUDEDIRS "." "tone")
register_component()
