
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
	lua ./tools/ldoc/ldoc.lua -d ./doc -c ./config.ld ejdb.luadoc

clean:
	- rm -f *.so *.rock ./ejdb/*.so
	- rm -rf ./doc
	- make -C ./test clean


.PHONY: all build build-dbg check check-valgrind clean doc
