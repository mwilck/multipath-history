# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
BUILD = glibc

include ../Makefile.inc

OBJS = main.o $(MULTIPATHLIB)-$(BUILD).a $(CHECKERSLIB)-$(BUILD).a

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = -I$(multipathdir) -I$(checkersdir)
	OBJS += $(SYSFSLIB)-$(BUILD).a $(DMLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -I$(multipathdir) -I$(checkersdir)
	LDFLAGS =

ifneq	($(WITH_LOCAL_LIBDM),)
	CFLAGS	+= -I$(libdmdir)
	OBJS	+= $(DMLIB)-$(BUILD).a
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

EXEC = multipath

all: $(BUILD)

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz
	
klibc: prepare $(OBJS)
	$(CC) -static -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

$(CHECKERSLIB)-$(BUILD).a:
	make -C $(checkersdir) BUILD=$(BUILD) $(BUILD)

$(MULTIPATHLIB)-$(BUILD).a:
	make -C $(multipathdir) BUILD=$(BUILD) $(BUILD)

$(SYSFSLIB)-$(BUILD).a:
	make -C $(sysfsdir) BUILD=$(BUILD) $(BUILD)

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD) $(BUILD)

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)/etc/dev.d/block/
	install -m 755 multipath.dev $(DESTDIR)/etc/dev.d/block/
	install -d $(DESTDIR)/etc/udev/rules.d
	install -m 755 multipath.rules $(DESTDIR)/etc/udev/rules.d/
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8.gz $(DESTDIR)$(mandir)

uninstall:
	rm $(DESTDIR)/etc/dev.d/block/multipath.dev
	rm $(DESTDIR)/etc/udev/rules.d/multipath.rules
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/$(EXEC).8.gz

clean:
	rm -f core *.o $(EXEC) *.gz
