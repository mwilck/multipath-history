BUILD = glibc
EXEC = multipathd

include ../Makefile.inc

#
# directories where to put stuff
#
bindir = /usr/bin
mandir = /usr/share/man/man8
rcdir = /etc/init.d

#
# basic flags setting
#
CFLAGS = -pipe -g -Wall -Wunused -Wstrict-prototypes \
	 -DDAEMON -I$(multipathdir) -I$(checkersdir)
LDFLAGS = -lpthread

#
# object files
#
OBJS = main.o copy.o log.o log_pthread.o \
       $(MULTIPATHLIB)-glibc.a \
       $(CHECKERSLIB)-glibc.a \

#
# Conditional flags for forced local libs.
#
ifneq	($(WITH_LOCAL_LIBDM),)
CFLAGS	+= -I$(libdmdir)
OBJS	+= $(DMLIB)-glibc.a
else
LDFLAGS	+= -ldevmapper
endif
ifneq	($(WITH_LOCAL_LIBSYSFS),)
CFLAGS	+= -I$(sysfsdir)
OBJS	+= $(SYSFSLIB)-glibc.a
else
LDFLAGS	+= -lsysfs
endif

#
# directives
#
all : $(BUILD)

glibc: $(EXEC)

klibc:
	$(MAKE) BUILD=glibc glibc

$(EXEC): clean $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	strip $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

$(CHECKERSLIB)-glibc.a:
	$(MAKE) -C $(checkersdir) BUILD=glibc glibc

$(MULTIPATHLIB)-glibc.a:
	$(MAKE) -C $(multipathdir) DAEMON=1 BUILD=glibc glibc

$(DMLIB)-glibc.a:
	$(MAKE) -C $(libdmdir) BUILD=glibc glibc

$(SYSFSLIB)-glibc.a:
	$(MAKE) -C $(sysfsdir) BUILD=glibc glibc

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(rcdir)
	install -m 755 multipathd.init $(DESTDIR)$(rcdir)/$(EXEC)
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8.gz $(DESTDIR)$(mandir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(rcdir)/$(EXEC)
	rm -f $(DESTDIR)$(mandir)/$(EXEC).8.gz

clean:
	$(MAKE) -C $(multipathdir) clean
	rm -f core *.o $(EXEC) *.gz

