EXEC        = dmadm

prefix      =
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
udevdir     = ../../..
klibcdir    = $(udevdir)/klibc
sysfsdir    = $(udevdir)/libsysfs
arch        = i386
klibcarch   = $(klibcdir)/klibc/arch/$(arch)/include

CC = gcc
GCCINCDIR := ${shell $(CC) -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp"}
KERNEL_DIR = /lib/modules/${shell uname -r}/build
CFLAGS = -pipe -g -O2 -Wall -Wunused -Wstrict-prototypes -nostdinc \
	 -I$(klibcdir)/klibc/include -I$(klibcdir)/klibc/include/bits32 \
	 -I$(GCCINCDIR) -I$(KERNEL_DIR)/include -I. -I$(klibcarch) -I$(sysfsdir)

OBJS = util.o dmadm.o
CRT0 = $(udevdir)/klibc/klibc/crt0.o
LIB = $(udevdir)/klibc/klibc/libc.a
LIBGCC := $(shell $(CC) -print-libgcc-file-name )

SYSFSOBJS = $(sysfsdir)/dlist.o $(sysfsdir)/sysfs_bus.o \
	    $(sysfsdir)/sysfs_class.o $(sysfsdir)/sysfs_device.o \
	    $(sysfsdir)/sysfs_dir.o $(sysfsdir)/sysfs_driver.o \
	    $(sysfsdir)/sysfs_utils.o

DMOBJS = ../libdevmapper/libdm-common.o \
	 ../libdevmapper/ioctl/libdevmapper.o

$(EXEC): $(OBJS)
	$(LD) -o $(EXEC) $(CRT0) $(OBJS) $(SYSFSOBJS) $(DMOBJS) $(LIB) $(LIBGCC)
	strip $(EXEC)

clean:
	rm -f $(OBJS) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJS): mdadm.h
