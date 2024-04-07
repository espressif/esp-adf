idf_component_register(
    SRCS
        "audio_mixer.c"
    INCLUDE_DIRS
        "include"
    REQUIRES    "audio_sal" "esp-adf-libs")