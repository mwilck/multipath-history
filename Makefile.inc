# Makefile.inc
#
# Copyright (C) 2004 Christophe Varoqui, <christophe.varoqui@free.fr>

SRCROOT=../klibc

include ${SRCROOT}/MCONFIG

prefix      = 
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
libdmdir    = ../libdevmapper
sysfsdir    = ../libsysfs
checkersdir = ../libcheckers
mandir      = /usr/share/man/man8

GZIP        = /bin/gzip -9 -c
STRIP       = strip --strip-all -R .comment -R .note

DMOBJS = $(libdmdir)/libdm-common.o \
	 $(libdmdir)/ioctl/libdm-iface.o

SYSFSOBJS = $(sysfsdir)/dlist.o \
	    $(sysfsdir)/sysfs_dir.o \
            $(sysfsdir)/sysfs_utils.o

CHECKERSOBJS = $(checkersdir)/selector.o \
               $(checkersdir)/tur.o \
	       $(checkersdir)/readsector0.o \
	       $(checkersdir)/emc_clariion.o \
	       $(checkersdir)/checkers.o
