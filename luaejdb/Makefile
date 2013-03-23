
UMASK=022

all: doc build

build:
	umask $(UMASK) && luarocks --pack-binary-rock make

build-dbg:
	umask $(UMASK) && luarocks --pack-binary-rock CFLAGS='-g -O0 -fPIC -std=c99 -Wall' make

check: ;

check-binding: build-dbg
	make -C ./test

check-valgrind: build-dbg
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check  -C ./test

doc:
	rm -rf ./doc
	lua ./tools/ldoc/ldoc.lua -d ./doc -c ./config.ld ejdb.luadoc

clean:
	- rm -f *.so *.rock ./ejdb/*.so
	- make -C ./test clean

install:
	$(if $(DESTDIR), $(MAKE) install-deb, $(MAKE) install-ndeb)

install-deb:
	install -d $(DESTDIR)/usr
	umask $(UMASK) && luarocks --tree=$(DESTDIR)/usr make

install-ndeb:
	umask $(UMASK) && luarocks make

deb-packages: clean
	debuild --no-tgz-check $(DEBUILD_OPTS)

.PHONY: all build build-dbg check check-valgrind clean doc install install-ndeb install-deb
