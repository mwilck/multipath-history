BUILD = klibc

EXEC = dmadm

OBJS = util.o dmadm.o

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS =  ${REQFLAGS} ${OPTFLAGS} -I../libsysfs \
		  -I../libdevmapper
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -DDEBUG=$(DEBUG)
	LDFLAGS = -ldevmapper -lsysfs
endif

all: klibc

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)

klibc: prepare $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSLIB)-$(BUILD).a \
	$(DMLIB)-$(BUILD).a $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)

clean:
	rm -f $(OBJS) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJS): mdadm.h
