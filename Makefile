
all:
	cd tcejdb && ./configure
	$(MAKE) -C ./tcejdb

clean:
	- $(MAKE) -C ./tcejdb clean
	- rm -rf ./build
	- rm -rf ./var/*
	- rm -f libtcejdb*.tar.gz libtcejdb*.deb libtcejdb*.changes libtcejdb*.build libtcejdb*.dsc

 deb-packages:
	cp ./Changelog ./tcejdb/Changelog
	cd ./tcejdb && autoconf && ./configure
	$(MAKE) -C ./tcejdb deb-packages

 deb-source-packages:
	$(MAKE) -C ./ deb-packages DEBUILD_OPTS="-S"


.PHONY: all clean