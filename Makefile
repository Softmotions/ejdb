
all:
	cd tcejdb && ./configure
	make -C ./tcejdb

.PHONY: all