# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD = glibc

include ../Makefile.inc

OBJS = memory.o parser.o vector.o devmapper.o callout.o \
       hwtable.o blacklist.o util.o dmparser.o config.o \
       structs.o cache.o discovery.o propsel.o dict.o \
       pgpolicies.o

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS += -I/usr/include
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes
endif

ifeq ($(strip $(DAEMON)),1)
	CFLAGS += -DDAEMON
endif

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

klibc: prepare $(OBJS)
	ar rs libmultipath-klibc.a *.o

glibc: prepare $(OBJS)
	ar rs libmultipath-glibc.a *.o

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
