idf_component_register(
    SRCS
        "audio_mixer_pipeline.c"
    INCLUDE_DIRS
        "include"
    REQUIRES "audio_pipeline" "audio_stream" "audio_mixer")