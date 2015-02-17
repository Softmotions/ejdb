#!/usr/bin/make -f

# Uncomment this to turn on verbose mode.
#export DH_VERBOSE=1

CFLAGS = -g -Wall -Wextra $(if $(findstring noopt,$(DEB_BUILD_OPTIONS)),-O0,-O2)
LDFLAGS= -Wl,-z,defs

DEB_BUILD_ARCH ?= $(shell dpkg-architecture -qDEB_BUILD_ARCH)
DEB_HOST_MULTIARCH ?= $(shell dpkg-architecture -qDEB_HOST_MULTIARCH)

%:
	dh $@

override_dh_auto_test:
	$(MAKE) check-ejdb

override_dh_auto_configure:
	dh_auto_configure -- \
	    $(if $(findstring $(DEB_BUILD_ARCH),s390 s390x),--disable-pthread) \
	    --enable-devel \
	    --libdir=\$${prefix}/lib/$(DEB_HOST_MULTIARCH) \
	    CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

clean:
	dh $@
	! test -e Makefile || $(MAKE) distclean
	rm -f build-*-stamp

override_dh_strip:
	dh_strip --dbg-package=libtcejdb9-dbg

.PHONY: override_dh_strip override_dh_auto_test override_dh_auto_configure clean
