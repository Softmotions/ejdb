
all:
	cd tcejdb && ./configure
	make -C ./tcejdb

clean:
	make -C ./tcejdb clean
	rm -rf ./build
	rm -rf ./var/*

.PHONY: all