# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

EXEC = multipath

include ../Makefile.inc

CFLAGS += -I$(klibcdir)/klibc/arch/$(arch)/include

OBJS = pgpolicies.o configfile.o devinfo.o main.o

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSOBJS) $(DMOBJS) $(LIB) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

clean:
	rm -f core *.o $(EXEC) *.gz

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)/etc/hotplug.d/scsi/
	install -m 755 multipath.hotplug $(DESTDIR)/etc/hotplug.d/scsi/
	install -d $(DESTDIR)$(mandir)
	install -m 644 multipath.8.gz $(DESTDIR)$(mandir)

uninstall:
	rm $(DESTDIR)/etc/hotplug.d/scsi/multipath.hotplug
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/multipath.8.gz

# Code dependencies
main.o: main.c main.h sg_include.h devinfo.h
