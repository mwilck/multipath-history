# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILD = klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS}
else
	CFLAGS =
endif

OBJS = dlist.o sysfs_dir.o sysfs_utils.o

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

glibc:
	
klibc: prepare $(KLIBC) $(OBJS)
	ar rs libsysfs-klibc.a *.o

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
