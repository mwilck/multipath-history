# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILD = klibc

include ../Makefile.inc

OBJS = dlist.o sysfs_dir.o sysfs_utils.o

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	ar rs libsysfs-glibc.a *.o
	
klibc: prepare $(OBJS)
	ar rs libsysfs-klibc.a *.o

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
