#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS := .
COMPONENT_ADD_INCLUDEDIRS := include 
COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/lib -lbds_private_info

