EXEC = multipathd

CC = gcc
GZIP = /bin/gzip -9 -c

bindir = /usr/bin
mandir = /usr/share/man/man8
rcdir = /etc/init.d

CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes -DDEBUG=1 -DFLOATING_STACKS
#LDFLAGS = -lpthread -ldevmapper -lsysfs
LDFLAGS = -lpthread -ldevmapper

OBJS = main.o devinfo.o checkers.o parser.o vector.o dict.o memory.o libsysfs/libsysfs.a

all: sysfs $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	strip $(EXEC)

sysfs:
	tar cf - ../libsysfs/ --exclude "*.[oa]" --exclude Makefile|tar xvf -
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

