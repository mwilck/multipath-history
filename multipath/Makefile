# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

EXEC = multipath
DEBUG = 0

include ../Makefile.inc

CFLAGS = ${REQFLAGS} ${OPTFLAGS} -DDEBUG=${DEBUG}

OBJS = pgpolicies.o devinfo.o main.o callout.o \
       vector.o parser.o dict.o memory.o

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSOBJS) $(DMOBJS) \
		 $(CHECKERSOBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

clean:
	rm -f core *.o $(EXEC) *.gz

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

# Code dependencies
main.o: main.c main.h sg_include.h devinfo.h
