# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

KERNEL_BUILD = /lib/modules/$(shell uname -r)/build
VERSION = $(shell basename ${PWD} | cut -d'-' -f3)

BUILDDIRS = klibc libsysfs libdevmapper \
          devmap_name multipath multipathd kpartx

INSTALLDIRS = devmap_name multipath multipathd kpartx

recurse:
	$(shell ln -s ${KERNEL_BUILD} klibc/linux)
	@for dir in $(BUILDDIRS); do\
	$(MAKE) -C $$dir ; \
	done

recurse_clean:
	@for dir in $(BUILDDIRS); do\
	$(MAKE) -C $$dir clean ; \
	done
	$(MAKE) -C klibc spotless

recurse_install:
	@for dir in $(INSTALLDIRS); do\
	$(MAKE) -C $$dir install ; \
	done

recurse_uninstall:
	@for dir in $(INSTALLDIRS); do\
	$(MAKE) -C $$dir uninstall ; \
	done

all: recurse

clean:	recurse_clean
	rm -rf rpms
	rm -f multipath-tools.spec

install:	recurse_install

uninstall:	recurse_uninstall

rpm:
	sed -e "s/__VERSION__/${VERSION}/" \
	multipath-tools.spec.in > multipath-tools.spec
	rpmbuild -bb multipath-tools.spec
