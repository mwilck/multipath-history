# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

include ../Makefile.inc

CFLAGS += -Iioctl

OBJS = ioctl/libdevmapper.o libdm-common.o

all:	$(OBJS)
	@echo ""
	@echo "Make complete"

install:

uninstall:

clean:
	rm -f core *.o ioctl/*.o ioctl/*.so
