# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILD = klibc

KERNEL_BUILD = /lib/modules/$(shell uname -r)/build

ifeq ($(strip $(BUILD)),klibc)
	BUILDDIRS = libsysfs libdevmapper libcheckers \
		    libmultipath \
		    devmap_name multipath multipathd kpartx
else
	BUILDDIRS = libsysfs libmultipath libcheckers \
		    devmap_name multipath multipathd kpartx
endif

VERSION = $(shell basename ${PWD} | cut -d'-' -f3)
INSTALLDIRS = devmap_name multipath multipathd kpartx

all: recurse

recurse:
	@if [ $(BUILD) = klibc ]; then\
	ln -s ${KERNEL_BUILD} klibc/linux ; \
	$(MAKE) -C klibc ; \
	fi
	@for dir in $(BUILDDIRS); do\
	$(MAKE) -C $$dir $(BUILD) BUILD=$(BUILD) VERSION=$(VERSION); \
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

clean:	recurse_clean
	rm -f klibc/linux
	rm -rf rpms
	rm -rf debian/tmp debian/stampdir

install:	recurse_install

uninstall:	recurse_uninstall

release:
	sed -e "s/__VERSION__/${VERSION}/" \
	multipath-tools.spec.in > multipath-tools.spec
	sed -e "s/__VERSION__/${VERSION}/" \
	debian/changelog.in > debian/changelog

rpm: release
	rpmbuild -bb multipath-tools.spec

deb: release
	dpkg-buildpackage

