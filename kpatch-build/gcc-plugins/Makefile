HOST_GCC	= g++
TARGET_GCC	= gcc
GCCPLUGINS_DIR := $(shell $(TARGET_GCC) -print-file-name=plugin)
CFLAGS	       += -I$(GCCPLUGINS_DIR)/include -fPIC -fno-rtti -O2 -Wall

ppc64le-plugin.so: ppc64le-plugin.c
	$(HOST_GCC) -shared $(CFLAGS) $^ -o $@
