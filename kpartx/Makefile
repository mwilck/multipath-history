EXEC        = kpartx

include ../Makefile.inc

OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o lopart.o xstrncpy.o
CFLAGS = -I. ${REQFLAGS} ${OPTFLAGS}

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(DMOBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)

clean:
	rm -f $(OBJS) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJS): kpartx.h
