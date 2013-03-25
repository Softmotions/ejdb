#================================================================
# Setting Variables
#================================================================


# Generic settings
SHELL = /bin/bash

SRCDIR = ./src/cpp/
DESTDIR = ./target/

# Building configuration
CC = gcc
CPPFLAGS = -I. -I../tcejdb -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux
CFLAGS =  -std=c99 -Wall -fPIC -fsigned-char -O2
LDFLAGS = -L. -L../tcejdb
CMDLDFLAGS =
LIBS = -lz -lrt -lpthread -lm -lc -shared -ltcejdb
RUNENV = LD_LIBRARY_PATH=.
POSTCMD = true


all: jejdb tests

jejdb:
	mkdir -p $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(DESTDIR)libjejdb.so $(SRCDIR)jejdb.c  $(LIBS)
	ant build

tests:
	ant build.and.test

clean:
	rm -rf  $(DESTDIR)libjejdb*

.PHONY: all jejdb tests clean
