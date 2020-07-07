# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly

set(COMPONENT_SRCS "esp_dlna.c" "esp_dlna_avt.c" "esp_dlna_cmr.c" "esp_dlna_rcs.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_EMBED_TXTFILES 	xml/ConnectionManager.xml
                                xml/AVTransport.xml
                                xml/RenderingControl.xml
                                xml/EventLastChange.xml
                                xml/EventProtocolCMR.xml)

set(COMPONENT_REQUIRES esp-adf-libs esp_http_server audio_sal)

register_component()