# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILD = klibc

include ../Makefile.inc

CFLAGS += -I. -Iioctl -Iinclude ${REQFLAGS} ${OPTFLAGS}

OBJS = ioctl/libdm-iface.o libdm-common.o

all: klibc

klibc: $(KLIBC) $(OBJS)
	ar rs libdevmapper-klibc.a *.o ioctl/*.o

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

install:

uninstall:

clean:
	rm -f core *.o ioctl/*.o ioctl/*.so *.a
