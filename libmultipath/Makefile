# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD = glibc

include ../Makefile.inc

OBJS = memory.o parser.o vector.o devmapper.o callout.o \
       sysfs_devinfo.o hwtable.o blacklist.o util.o

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -I$(libdmdir)
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes
endif

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

klibc: prepare $(KLIBC) $(OBJS)
	ar rs libmultipath-klibc.a *.o

glibc: prepare $(OBJS)
	ar rs libmultipath-glibc.a *.o

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
