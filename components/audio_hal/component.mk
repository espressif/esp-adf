#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


COMPONENT_ADD_INCLUDEDIRS := ./include ./driver/es8388 ./board ./driver/es8374 ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware

COMPONENT_SRCDIRS :=  . ./driver/es8388 ./board ./driver/es8374 ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware

COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/driver/zl38063/firmware -lfirmware

