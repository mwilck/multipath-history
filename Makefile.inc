# Makefile.inc
#
# Copyright (C) 2004 Christophe Varoqui, <christophe.varoqui@free.fr>

ifeq ($(strip $(BUILD)),klibc)
	SRCROOT=../klibc
	include ${SRCROOT}/MCONFIG
endif

prefix      = 
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
libdmdir    = ../libdevmapper
sysfsdir    = ../libsysfs
checkersdir = ../libcheckers
mandir      = /usr/share/man/man8

GZIP        = /bin/gzip -9 -c
STRIP       = strip --strip-all -R .comment -R .note

DMLIB = $(libdmdir)/libdevmapper
SYSFSLIB = $(sysfsdir)/libsysfs
CHECKERSLIB = $(checkersdir)/libcheckers
