set(COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_SRCS "recorder_sr.c"
                    "recorder_encoder.c"
                    "audio_recorder.c")

set(COMPONENT_REQUIRES audio_sal esp-sr audio_pipeline)

register_component()
