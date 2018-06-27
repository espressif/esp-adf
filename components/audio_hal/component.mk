#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


COMPONENT_ADD_INCLUDEDIRS := ./include ./driver/es8388 ./board ./driver/es8374 ./driver/wm8978


#COMPONENT_SRCDIRS :=. ./driver/es8388 ./board ./driver/es8374 ./driver/wm8978
ifdef CONFIG_WHYENGINEER_LIN_BOARD
COMPONENT_SRCDIRS := . ./driver/wm8978
else
COMPONENT_SRCDIRS := . ./driver/es8388 ./board ./driver/es8374
endif
