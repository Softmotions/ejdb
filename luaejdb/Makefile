CFLAGS=-g -O0 -fPIC -std=c99 -Wall -D_GNU_SOURCE

build:
	luarocks --pack-binary-rock CFLAGS='$(CFLAGS)' make

check: build
	make -C ./test

check-valgrind: build
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check  -C ./test

clean:
	rm -rf *.so *.rock
	make -C ./test clean


.PHONY: build test clean
