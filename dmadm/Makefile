BUILD = glibc

EXEC = dmadm

include ../Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS =  ${REQFLAGS} ${OPTFLAGS} -I$(sysfsdir) -I$(libdmdir)
	OBJS = util.o dmadm.o $(SYSFSLIB)-$(BUILD).a $(DMLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes
	LDFLAGS =
	OBJS = util.o dmadm.o

ifneq	($(WITH_LOCAL_LIBDM),)
	CFLAGS	+= -I$(libdmdir)
	OBJS	+=  $(DMLIB)-$(BUILD).a
else
	LDFLAGS	+= -ldevmapper
endif
ifneq	($(WITH_LOCAL_LIBSYSFS),)
	CFLAGS	+= -I$(sysfsdir)
	OBJS	+= $(SYSFSLIB)-$(BUILD).a
else
	LDFLAGS	+= -lsysfs
endif
endif

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)

klibc: prepare $(KLIBC) $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)

$(SYSFSLIB)-$(BUILD).a:
	make -C $(sysfsdir) BUILD=$(BUILD) $(BUILD)

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD) $(BUILD)

$(KLIBC):
	rm -f $(klibcdir)/linux
	ln -s $(KERNEL_BUILD) $(klibcdir)/linux
	make -C $(klibcdir)

clean:
	rm -f $(OBJS) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJS): mdadm.h
