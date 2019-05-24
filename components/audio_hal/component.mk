#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include

COMPONENT_ADD_INCLUDEDIRS +=  ./driver/es8388  ./board/es8374
COMPONENT_SRCDIRS += ./driver/es8388 ./board/es8374

COMPONENT_ADD_INCLUDEDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware
COMPONENT_SRCDIRS += ./driver/zl38063 ./driver/zl38063/api_lib ./driver/zl38063/example_apps ./driver/zl38063/firmware

