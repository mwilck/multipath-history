# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
BUILD = klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -I$(libdmdir)
	OBJS = devmap_name.o $(DMLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -DDEBUG=$(DEBUG)
	LDFLAGS = -ldevmapper
	OBJS = devmap_name.o
endif

EXEC = devmap_name

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(KLIBC) $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD) $(BUILD)

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

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
