# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
BUILD = klibc

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = ${REQFLAGS} ${OPTFLAGS} -DDEBUG=${DEBUG} \
		 -I../libdevmapper -I../libsysfs -I../libcheckers \
		 -I../libmultipath
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -DDEBUG=$(DEBUG) -I../libmultipath -I../libcheckers
	LDFLAGS = -ldevmapper -lsysfs
endif

EXEC = multipath
DEBUG = 0

OBJS = pgpolicies.o devinfo.o main.o callout.o dict.o \
       ../libmultipath/libmultipath-$(BUILD).a \
       ../libcheckers/libcheckers-$(BUILD).a

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSLIB)-$(BUILD).a \
	$(DMLIB)-$(BUILD).a $(CHECKERSLIB)-$(BUILD).a $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)/etc/dev.d/block/
	install -m 755 multipath.dev $(DESTDIR)/etc/dev.d/block/
	install -d $(DESTDIR)/etc/udev/rules.d
	install -m 755 multipath.rules $(DESTDIR)/etc/udev/rules.d/
	install -d $(DESTDIR)$(mandir)
	install -m 644 multipath.8.gz $(DESTDIR)$(mandir)

uninstall:
	rm $(DESTDIR)/etc/dev.d/block/multipath.dev
	rm $(DESTDIR)/etc/udev/rules.d/multipath.rules
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/multipath.8.gz

clean:
	rm -f core *.o $(EXEC) *.gz
