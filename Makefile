
all:
	cd tcejdb && ./configure
	make -C ./tcejdb

clean:
	make -C ./tcejdb clean
	rm -rf ./build
	rm -rf ./var/*


 deb-packages:
	cd ./tcejdb && autoconf && ./configure
	make -C ./tcejdb deb-packages



.PHONY: all clean