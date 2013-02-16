
all: fix-changelogs
	cd tcejdb && ./configure
	$(MAKE) -C ./tcejdb

clean:
	- $(MAKE) -C ./tcejdb clean
	- rm -rf ./build
	- rm -rf ./var/*
	- rm -f *.upload
	- rm -f libtcejdb*.tar.gz libtcejdb*.deb libtcejdb*.changes libtcejdb*.build libtcejdb*.dsc
	- rm -f python*.tar.gz python*.deb python*.changes python*.build python*.dsc	

deb-packages: fix-changelogs
	cd ./tcejdb && autoconf && ./configure
	$(MAKE) -C ./tcejdb deb-packages
	$(MAKE) -C ./pyejdb deb-packages

deb-source-packages: fix-changelogs
	$(MAKE) -C ./ deb-packages DEBUILD_OPTS="-S"


fix-changelogs:
	cp ./Changelog ./tcejdb/Changelog


.PHONY: all clean deb-packages deb-source-packages fix-changelogs
