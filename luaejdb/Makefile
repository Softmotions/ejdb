
all: doc build

build:
	luarocks --pack-binary-rock make

build-dbg:
	luarocks --pack-binary-rock CFLAGS='-g -O0 -fPIC -std=c99 -Wall' make

check: build-dbg
	make -C ./test

check-valgrind: build-dbg
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check  -C ./test

doc:
	rm -rf ./doc
	luadoc -d ./doc ejdb.luadoc

clean:
	- rm -rf *.so *.rock
	- rm -rf ./doc
	- make -C ./test clean


.PHONY: all build build-dbg check check-valgrind clean doc
