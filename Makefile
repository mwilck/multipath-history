# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>

SUBDIRS = klibc libsysfs libdevmapper \
          devmap_name multipath multipathd kpartx

recurse:
	@for dir in $(SUBDIRS); do\
	$(MAKE) -C $$dir ; \
	done

recurse_clean:
	@for dir in $(SUBDIRS); do\
	$(MAKE) -C $$dir clean ; \
	done

recurse_install:
	@for dir in $(SUBDIRS); do\
	$(MAKE) -C $$dir install ; \
	done

recurse_uninstall:
	@for dir in $(SUBDIRS); do\
	$(MAKE) -C $$dir uninstall ; \
	done

all:	recurse
	@echo ""
	@echo "Make complete"

clean:	recurse_clean
	@echo ""
	@echo "Make complete"
	rm -rf rpms

install:	recurse_install
	@echo ""
	@echo "Make complete"

uninstall:	recurse_uninstall
	@echo ""
	@echo "Make complete"

rpm:
	rpmbuild -bb multipath-tools.spec
