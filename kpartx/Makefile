EXEC        = kpartx

prefix      =
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/sbin
udevdir     = ../../..
klibcdir    = $(udevdir)/klibc
arch        = i386
klibcarch   = $(klibcdir)/klibc/arch/$(arch)/include

CC = gcc
GCCINCDIR := ${shell $(CC) -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp"}
KERNEL_DIR = /lib/modules/${shell uname -r}/build
CFLAGS = -pipe -g -O2 -Wall -Wunused -Wstrict-prototypes -nostdinc \
	 -I$(klibcdir)/klibc/include -I$(klibcdir)/klibc/include/bits32 \
	 -I$(GCCINCDIR) -I$(KERNEL_DIR)/include -I. -I$(klibcarch)

OBJ = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o lopart.o xstrncpy.o
CRT0 = $(udevdir)/klibc/klibc/crt0.o
LIB = $(udevdir)/klibc/klibc/libc.a
LIBGCC := $(shell $(CC) -print-libgcc-file-name )

DMOBJS = ../libdevmapper/libdm-common.o \
	 ../libdevmapper/ioctl/libdevmapper.o

$(EXEC): $(OBJ)
	$(LD) -o $(EXEC) $(CRT0) $(OBJ) $(DMOBJS) $(LIB) $(LIBGCC)
	strip $(EXEC)

clean:
	rm -f $(OBJ) *.o $(EXEC) *~

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)

$(OBJ): kpartx.h
