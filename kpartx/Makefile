# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD=klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -I../libdevmapper -I.
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -I.
	LDFLAGS = -ldevmapper
endif

EXEC = kpartx
OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o lopart.o xstrncpy.o

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
#	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(DMLIB)-$(BUILD).a \
	$(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
#	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

clean:
	rm -f core *.o $(EXEC) *.gz
