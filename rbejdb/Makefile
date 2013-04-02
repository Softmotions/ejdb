
all: build check

build:
	export CFLAGS=-g
	ruby extconf.rb
	make -C ./build
	make install -C ./build

check:
	make -C ./test

doc:

clean:
	rm -rf build

.PHONY: all build check clean doc
