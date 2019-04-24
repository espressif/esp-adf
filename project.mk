ADF_VER := $(shell cd ${ADF_PATH} && git describe --always --tags --dirty)
ifndef IDF_PATH
IDF_PATH := $(ADF_PATH)/esp-idf
endif
EXTRA_COMPONENT_DIRS += $(ADF_PATH)/components/
CPPFLAGS := -D ADF_VER=\"$(ADF_VER)\"
include $(IDF_PATH)/make/project.mk
