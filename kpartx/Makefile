# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD=klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -I$(libdmdir) -I.
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o \
	       lopart.o xstrncpy.o $(DMLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -I.
	LDFLAGS = -ldevmapper
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o \
	       lopart.o xstrncpy.o
endif

EXEC = kpartx

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	
klibc: prepare $(KLIBC) $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD) $(BUILD)

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

clean:
	rm -f core *.o $(EXEC) *.gz
