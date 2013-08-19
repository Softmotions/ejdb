
all: init
	$(MAKE) -C ./tcejdb

clean:
	- $(MAKE) -C ./tcejdb clean
	- rm -rf ./build
	- rm -rf ./var/*
	- rm -f *.upload
	- rm -f libtcejdb*.tar.gz libtcejdb*.deb libtcejdb*.changes libtcejdb*.build libtcejdb*.dsc
	- rm -f python*.tar.gz python*.deb python*.changes python*.build python*.dsc
	- rm -f lua*.tar.gz lua*.deb lua*.changes lua*.build lua*.dsc
	- rm -f *.tgz

deb-packages: deb-packages-tcejdb deb-packages-pyejdb deb-packages-luaejdb;

deb-packages-tcejdb: init
	$(MAKE) -C ./tcejdb deb-packages

deb-packages-pyejdb: init
	$(MAKE) -C ./pyejdb deb-packages

deb-packages-luaejdb: init
	$(MAKE) -C ./luaejdb deb-packages

deb-source-packages:
	$(MAKE) -C ./ deb-packages DEBUILD_OPTS="-S"

deb-source-packages-tcejdb:
	$(MAKE) -C ./ deb-packages-tcejdb DEBUILD_OPTS="-S"

deb-source-packages-pyejdb:
	$(MAKE) -C ./ deb-packages-pyejdb DEBUILD_OPTS="-S"

deb-source-packages-luaejdb:
	$(MAKE) -C ./ deb-packages-luaejdb DEBUILD_OPTS="-S"

init:
	cd ./tcejdb && ./configure
	$(MAKE) -C ./tcejdb version
	- cp ./tcejdb/debian/changelog ./Changelog 
	- cp ./tcejdb/debian/changelog ./tcejdb/Changelog

.PHONY: all clean deb-packages deb-source-packages init initdeb
