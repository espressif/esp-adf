#
# Main Makefile. This is basically the same as a component makefile.

COMPONENT_ADD_INCLUDEDIRS := include . "cnv_basic" "cnv_audio"
COMPONENT_PRIV_INCLUDEDIRS := .
COMPONENT_SRCDIRS := .
COMPONENT_REQUIRES := audio_stream esp-dsp utilis
