
all: build doc

build:
	mkdir -p build
	cd ./build; ruby ../extconf.rb
	make -C ./build

install:
	make install -C ./build

check:
	make -C ./test

doc:
	rdoc src

build-gem:
	gem build rbejdb.gemspec

clean:
	rm -rf build
	rm -rf doc
	rm -rf test/testdb
	rm -f mkmf.log
	rm -f rbejdb*.gem

.PHONY: all build install check doc build-gem clean
