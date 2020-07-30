#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_ADD_LDFLAGS += $(COMPONENT_PATH)/lib/libbdsc_engine.a

CFLAGS += -D APP_VER=\"$(shell git rev-parse HEAD)\"
COMPONENT_EMBED_TXTFILES := server_root_cert.pem
