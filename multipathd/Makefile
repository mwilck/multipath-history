EXEC = multipathd

#
# debug verbosity, same as syslog.h
# nothing to below 5, full debug at 7
#
LOGLEVEL = 5

include ../Makefile.inc

#
# directories where to put stuff
#
bindir = /usr/bin
mandir = /usr/share/man/man8
rcdir = /etc/init.d

#
# basic flags setting
#
CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -DLOGLEVEL=$(LOGLEVEL) \
	 -DFLOATING_STACKS -I$(multipathdir) -I$(checkersdir) -I$(sysfsdir)
LDFLAGS = -lpthread -ldevmapper

#
# detect arch
#
ARCH = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/)
CFLAGS += -D${ARCH}

#
# object files
#
OBJS = main.o copy.o devinfo.o callout.o dict.o \
       $(CHECKERSLIB)-glibc.a \
       $(MULTIPATHLIB)-glibc.a \
       $(SYSFSLIB)-glibc.a

#
# directives
#
all : glibc

glibc: $(EXEC)

klibc: glibc

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	strip $(EXEC)

$(CHECKERSLIB)-glibc.a:
	$(MAKE) -C $(checkersdir) BUILD=glibc glibc

$(MULTIPATHLIB)-glibc.a:
	$(MAKE) -C $(multipathdir) BUILD=glibc glibc

$(SYSFSLIB)-glibc.a:
	$(MAKE) -C $(sysfsdir) BUILD=glibc glibc

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(rcdir)
	install -m 755 multipathd.init $(DESTDIR)$(rcdir)/$(EXEC)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(rcdir)/$(EXEC)

clean:
	rm -f core *.o $(EXEC) *.gz
	rm -rf libsysfs
	rm -rf libcheckers

