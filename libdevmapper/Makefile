# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILD = klibc

include ../Makefile.inc

CFLAGS += -I. -Iioctl -Iinclude

ifeq	($(strip $(BUILD)),glibc)
ifneq	($(WITH_LOCAL_LIBDM),)
CFLAGS	+= -I$(KRNLSRC)/include -I$(KRNLOBJ)/include -I$(KRNLOBJ)/include2
endif
endif

OBJS = ioctl/libdm-iface.o libdm-common.o

all: $(BUILD)

prepare:
	rm -f *.o ioctl/*.o

klibc: prepare $(OBJS)
	ar rs libdevmapper-klibc.a *.o ioctl/*.o

glibc: prepare $(OBJS)
	ar rs libdevmapper-glibc.a *.o ioctl/*.o

install:

uninstall:

clean:
	rm -f core *.o ioctl/*.o ioctl/*.so *.a
