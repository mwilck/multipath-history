# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD=glibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = -I$(libdmdir) -I.
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o \
	       lopart.o xstrncpy.o devmapper.o \
	       $(MULTIPATHLIB)-$(BUILD).a $(DMLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -I.
	LDFLAGS = -ldevmapper
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o \
	       gpt.o crc32.o lopart.o xstrncpy.o devmapper.o

ifneq	($(WITH_LOCAL_LIBDM),)
	CFLAGS	+= -I$(libdmdir)
	OBJS	+= $(DMLIB)-$(BUILD).a
else
	LDFLAGS	+= -ldevmapper
endif
endif

EXEC = kpartx

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(OBJS)
	$(CC) -static -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD)

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8.gz $(DESTDIR)$(mandir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(mandir)/$(EXEC).8.gz

clean:
	rm -f core *.o $(EXEC) *.gz
