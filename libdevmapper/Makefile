# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

include ../Makefile.inc

CFLAGS += -I. -Iioctl -Iinclude ${REQFLAGS} ${OPTFLAGS}

OBJS = ioctl/libdm-iface.o libdm-common.o

all:	$(OBJS)

install:

uninstall:

clean:
	rm -f core *.o ioctl/*.o ioctl/*.so
