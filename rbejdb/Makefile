
all: build doc

build:
	ruby extconf.rb
	make -C ./build

install:
	make install -C ./build

check:
	make -C ./test

doc:
	rdoc src

clean:
	rm -rf build
	rm -rf doc
	rm -rf test/testdb
	rm -f mkmf.log

.PHONY: all build install check doc clean
