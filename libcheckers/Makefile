# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD = klibc

include ../Makefile.inc

OBJS = readsector0.o tur.o selector.o checkers.o emc_clariion.o

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS}
else
	CFLAGS =
endif

all: klibc

prepare:
	rm -f core *.o *.gz

klibc: prepare $(KLIBC) $(OBJS)
	ar rs libcheckers-klibc.a *.o

glibc: prepare $(OBJS)
	ar rs libcheckers-glibc.a *.o

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
