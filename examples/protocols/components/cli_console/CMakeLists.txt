set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS src/console.c)

set(COMPONENT_REQUIRES console audio_sal esp_wifi driver esp_timer)

register_component()