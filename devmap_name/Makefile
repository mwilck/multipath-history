# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
BUILD = klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -I../libdevmapper
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -DDEBUG=$(DEBUG)
	LDFLAGS = -ldevmapper
endif

EXEC = devmap_name
OBJS = devmap_name.o

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(DMLIB)-$(BUILD).a \
	$(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8.gz $(DESTDIR)$(mandir)

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/$(EXEC).8.gz

clean:
	rm -f core *.o $(EXEC) *.gz
