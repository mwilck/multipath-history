# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
BUILD = glibc
DEBUG = 0

TOPDIR	= ..
include $(TOPDIR)/Makefile.inc

ifeq ($(strip $(BUILD)),klibc)
	CFLAGS = -I$(libdmdir) -I$(multipathdir) -DDEBUG=$(DEBUG)
	OBJS = pp_balance_units.o $(DMLIB)-$(BUILD).a \
	       $(MULTIPATHLIB)-$(BUILD).a
else
	CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
		 -I$(multipathdir) -DDEBUG=$(DEBUG)
	LDFLAGS = -ldevmapper
	OBJS = pp_balance_units.o $(MULTIPATHLIB)-$(BUILD).a
endif

EXEC = pp_balance_units
SUBDIRS	= $(shell find . -type d -mindepth 1 -maxdepth 1|cut -c3-)

all: $(BUILD)
	@for DIR in $(SUBDIRS); do \
		$(MAKE) -C $$DIR BUILD=$(BUILD) VERSION=$(VERSION); \
	done

prepare:
	rm -f core *.o *.gz

glibc: prepare $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LDFLAGS)
	$(STRIP) $(EXEC)
	
klibc: prepare $(OBJS)
	$(CC) -static -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)
	$(STRIP) $(EXEC)

$(DMLIB)-$(BUILD).a:
	make -C $(libdmdir) BUILD=$(BUILD) $(BUILD)

$(MULTIPATHLIB)-$(BUILD).a:
	make -C $(multipathdir) BUILD=$(BUILD) $(BUILD)

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	@for DIR in $(SUBDIRS); do \
		$(MAKE) -C $$DIR install; \
	done

uninstall:
	@for DIR in $(SUBDIRS); do \
		$(MAKE) -C $$DIR uninstall; \
	done
	rm $(DESTDIR)$(bindir)/$(EXEC)

clean:
	@for DIR in $(SUBDIRS); do \
		$(MAKE) -C $$DIR clean; \
	done
	rm -f core *.o $(EXEC) *.gz
