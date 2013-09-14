
all: init
	$(MAKE) -C ./tcejdb

clean:
	- $(MAKE) -C ./tcejdb clean
	- rm -f *.upload
	- rm -f libtcejdb*.tar.gz libtcejdb*.deb libtcejdb*.changes libtcejdb*.build libtcejdb*.dsc
	- rm -f *.tgz

deb-packages: deb-packages-tcejdb;

deb-packages-tcejdb: init
	$(MAKE) -C ./tcejdb deb-packages

deb-source-packages:
	$(MAKE) -C ./ deb-packages DEBUILD_OPTS="-S"

deb-source-packages-tcejdb:
	$(MAKE) -C ./ deb-packages-tcejdb DEBUILD_OPTS="-S"

init:
	cd ./tcejdb && ./configure
	$(MAKE) -C ./tcejdb version
	- cp ./tcejdb/debian/changelog ./Changelog 
	- cp ./tcejdb/debian/changelog ./tcejdb/Changelog

.PHONY: all clean deb-packages deb-source-packages init initdeb
