EXEC = multipathd

#
# debug verbosity (1-3)
#
DEBUG = 1

#
# some binaries
#
CC = gcc
GZIP = /bin/gzip -9 -c

#
# directories where to put stuff
#
bindir = /usr/bin
mandir = /usr/share/man/man8
rcdir = /etc/init.d

#
# basic flags setting
#
CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -DDEBUG=$(DEBUG) \
	 -DFLOATING_STACKS
LDFLAGS = -lpthread -ldevmapper

#
# detect arch
#
ARCH = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/)
CFLAGS += -D${ARCH}

#
# object files
#
OBJS = main.o copy.o devinfo.o checkers.o parser.o callout.o \
       vector.o dict.o memory.o libsysfs/libsysfs.a

#
# directives
#
all: sysfs $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	strip $(EXEC)

sysfs:
	(cd ..;tar cf - libsysfs/ --exclude "*.[oa]" --exclude Makefile)|tar xvf -
	grep -v include ../libsysfs/Makefile > libsysfs/Makefile
	make -C libsysfs

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

