# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD = glibc

include ../Makefile.inc

OBJS = memory.o parser.o vector.o

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS}
else
	CFLAGS =
endif

prepare:
	rm -f core *.o *.gz

klibc: prepare $(OBJS)
	ar rs libmultipath-klibc.a *.o

glibc: prepare $(OBJS)
	ar rs libmultipath-glibc.a *.o

all: klibc

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
