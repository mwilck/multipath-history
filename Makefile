# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

EXEC = multipath

prefix      = 
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
udevdir	    = ../..
klibcdir    = $(udevdir)/klibc
sysfsdir    = $(udevdir)/libsysfs
mandir      = /usr/share/man/man8

CC = gcc
GZIP = /bin/gzip -c

GCCINCDIR := ${shell $(CC) -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp"}
KERNEL_DIR = /lib/modules/${shell uname -r}/build
CFLAGS = -pipe -g -O2 -Wall -Wunused -Wstrict-prototypes -nostdinc \
         -I$(klibcdir)/klibc/include -I$(klibcdir)/klibc/include/bits32 \
         -I$(GCCINCDIR) -I$(KERNEL_DIR)/include -I$(sysfsdir) -I.

OBJS = devinfo.o main.o
CRT0 = ../../klibc/klibc/crt0.o
LIB = ../../klibc/klibc/libc.a
LIBGCC := $(shell $(CC) -print-libgcc-file-name )

DMOBJS = libdevmapper/libdm-common.o libdevmapper/ioctl/libdevmapper.o
SYSFSOBJS = ../../libsysfs/dlist.o ../../libsysfs/sysfs_bus.o \
	    ../../libsysfs/sysfs_class.o ../../libsysfs/sysfs_device.o \
	    ../../libsysfs/sysfs_dir.o ../../libsysfs/sysfs_driver.o \
	    ../../libsysfs/sysfs_utils.o

SUBDIRS = libdevmapper

recurse:
	@for dir in $(SUBDIRS); do\
	$(MAKE) KERNEL_DIR=$(KERNEL_DIR) -C $$dir ; \
	done
	$(MAKE) $(EXEC)
	$(MAKE) devmap_name

all:	recurse
	@echo ""
	@echo "Make complete"

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSOBJS) $(DMOBJS) $(LIB) $(LIBGCC)
	strip $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

devmap_name: devmap_name.o
	$(LD) -o devmap_name $(CRT0) devmap_name.o $(DMOBJS) $(LIB) $(LIBGCC)
	strip devmap_name
	$(GZIP) devmap_name.8 > devmap_name.8.gz

clean:
	rm -f core *.o $(EXEC) devmap_name *.gz
	$(MAKE) -C libdevmapper clean

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -m 755 devmap_name $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)/etc/hotplug.d/scsi/
	install -m 755 multipath.hotplug $(DESTDIR)/etc/hotplug.d/scsi/
	install -d $(DESTDIR)$(mandir)
	install -m 644 devmap_name.8.gz $(DESTDIR)$(mandir)
	install -m 644 multipath.8.gz $(DESTDIR)$(mandir)

uninstall:
	rm $(DESTDIR)/etc/hotplug.d/scsi/multipath.hotplug
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(bindir)/devmap_name
	rm $(DESTDIR)$(mandir)/devmap_name.8.gz
	rm $(DESTDIR)$(mandir)/multipath.8.gz

# Code dependencies
main.o: main.c main.h sg_include.h devinfo.h
