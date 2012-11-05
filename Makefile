
all:
	cd tcejdb && ./configure --disable-bzip
	make -C ./tcejdb

clean:
	make -C ./tcejdb clean
	rm -rf ./build
	rm -rf ./var/*

.PHONY: all