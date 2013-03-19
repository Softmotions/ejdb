
build:
	luarocks --pack-binary-rock make

build-dbg:
	luarocks --pack-binary-rock CFLAGS='-g -O0 -fPIC -std=c99 -Wall' make

check: build-dbg
	make -C ./test

check-valgrind: build-dbg
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check  -C ./test

clean:
	rm -rf *.so *.rock
	make -C ./test clean


.PHONY: build build-dbg check check-valgrind clean
