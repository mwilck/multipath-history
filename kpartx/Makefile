EXEC        = kpartx

#klibcarch   = $(klibcdir)/klibc/arch/$(arch)/include

include ../Makefile.inc

OBJ = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o lopart.o xstrncpy.o

$(EXEC): $(OBJ)
	$(LD) -o $(EXEC) $(CRT0) $(OBJ) $(DMOBJS) $(LIB) $(LIBGCC)
	$(STRIP) $(EXEC)

clean:
	rm -f $(OBJ) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJ): kpartx.h
