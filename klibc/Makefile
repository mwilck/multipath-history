VERSION := $(shell cat version)
SUBDIRS = klibc

$(shell rm -f linux)
$(shell ln -s /lib/modules/$(shell uname -r)/build linux)

all:

rpmbuild = $(shell which rpmbuild 2>/dev/null || which rpm)

klibc.spec: klibc.spec.in version
	sed -e 's/@@VERSION@@/$(VERSION)/g' < $< > $@

.PHONY: rpm
rpm: klibc.spec
	+$(rpmbuild) -bb klibc.spec

%:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

install:

uninstall:

clean:
	$(shell rm -f linux)
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

spotless:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f klibc.spec *~ tags
