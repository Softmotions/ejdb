
all:
	python3 ./setup.py clean build sdist

clean:
	- rm -f ./testdb*
	- rm -rf ./dist

deb-packages: clean
	debuild --no-tgz-check $(DEBUILD_OPTS)

.PHONY: all clean deb-packages