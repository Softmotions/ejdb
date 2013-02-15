
all:
	./setup.py clean build sdist

clean:
	- rm -f ./testdb*
	- rm -f ./dist

deb-packages: clean
	debuild $(DEBUILD_OPTS)

.PHONY: all clean deb-packages