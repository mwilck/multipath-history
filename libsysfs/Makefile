# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

include ../Makefile.inc

CFLAGS = ${REQFLAGS} ${OPTFLAGS}

OBJS = dlist.o \
       sysfs_dir.o \
       sysfs_utils.o

all: $(OBJS)
	ar rs libsysfs.a *.o

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
