# Makefile.inc
#
# Copyright (C) 2004 Christophe Varoqui, <christophe.varoqui@free.fr>

# directories
prefix      = 
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
libdmdir    = ../libdevmapper
klibcdir    = ../klibc
sysfsdir    = ../libsysfs
mandir      = /usr/share/man/man8
arch        = ${shell $(CC) -dumpmachine | sed \
              -e s'/-.*//' -e 's/i.86/i386/' -e 's/sparc.*/sparc/' \
              -e 's/arm.*/arm/g' -e 's/m68k.*/m68k/' -e 's/powerpc/ppc/g'}

# tools
CC          = gcc
GZIP        = /bin/gzip -9 -c
STRIP       = strip --strip-all -R .comment -R .note

# arch specific stuff
include $(klibcdir)/klibc/arch/$(arch)/MCONFIG

# compilation params
GCCINCDIR := ${shell unset LANG;$(CC) -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp"}
KERNEL_DIR = /lib/modules/${shell uname -r}/build
CFLAGS = -pipe -g -O2 -Wall -Wunused -Wstrict-prototypes -nostdinc \
         -I$(klibcdir)/klibc/include \
	 -I$(klibcdir)/klibc/include/bits$(BITSIZE) \
         -I$(GCCINCDIR) -I$(KERNEL_DIR)/include -I$(sysfsdir) -I.

CRT0 = $(klibcdir)/klibc/crt0.o
LIB = $(klibcdir)/klibc/libc.a
LIBGCC := $(shell $(CC) -print-libgcc-file-name )

DMOBJS = $(libdmdir)/libdm-common.o $(libdmdir)/ioctl/libdm-iface.o
SYSFSOBJS = $(sysfsdir)/dlist.o $(sysfsdir)/sysfs_bus.o \
            $(sysfsdir)/sysfs_class.o $(sysfsdir)/sysfs_device.o \
            $(sysfsdir)/sysfs_dir.o $(sysfsdir)/sysfs_driver.o \
            $(sysfsdir)/sysfs_utils.o

