set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS src/console.c)

set(COMPONENT_REQUIRES console audio_sal)

register_component()