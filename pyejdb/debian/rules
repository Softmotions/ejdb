#!/usr/bin/make -f

PYTHON3=$(shell py3versions -vr)

%:
	dh $@ --with python3

build-python%:
	python$* setup.py build

override_dh_auto_build: $(PYTHON3:%=build-python%)
	dh_auto_build

install-python%:
	python$* setup.py install --root=$(CURDIR)/debian/tmp --install-layout=deb

override_dh_auto_install: $(PYTHON3:%=install-python%)
	dh_auto_install

override_dh_auto_clean:
	dh_auto_clean
	rm -rf build
	rm -rf *.egg-info
