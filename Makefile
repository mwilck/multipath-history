# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

BUILDDIRS = klibc libsysfs libdevmapper \
          devmap_name multipath multipathd kpartx

INSTALLDIRS = devmap_name multipath multipathd kpartx

recurse:
	@for dir in $(BUILDDIRS); do\
	$(MAKE) -C $$dir ; \
	done

recurse_clean:
	@for dir in $(BUILDDIRS); do\
	$(MAKE) -C $$dir clean ; \
	done

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

install:	recurse_install

uninstall:	recurse_uninstall

rpm:	rpmbuild -bb multipath-tools.spec
