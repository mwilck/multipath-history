# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

include ../Makefile.inc

CFLAGS = ${REQFLAGS} ${OPTFLAGS}

OBJS = readsector0.o tur.o selector.o checkers.o emc_clariion.o

all: $(OBJS)
	ar rs libcheckers.a *.o

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz
