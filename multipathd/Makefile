EXEC = multipathd

CC = gcc
GZIP = /bin/gzip -9 -c

bindir = /usr/bin
mandir = /usr/share/man/man8
rcdir = /etc/init.d

CFLAGS = -pipe -g -O2 -Wall -Wunused -Wstrict-prototypes -DDEBUG=1 -DFLOATING_STACKS
LDFLAGS = -lpthread -ldevmapper

OBJS = main.o devinfo.o checkers.o

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(EXEC)
	strip $(EXEC)

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

