EXEC        = dmadm

OBJS = util.o dmadm.o

include ../Makefile.inc

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSOBJS) $(DMOBJS) $(LIB) $(LIBGCC)
	$(STRIP) $(EXEC)

clean:
	rm -f $(OBJS) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJS): mdadm.h
