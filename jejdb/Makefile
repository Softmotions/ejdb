#================================================================
# Setting Variables
#================================================================


# Generic settings
SHELL = /bin/bash

# Package information
PACKAGE = tcejdb
VERSION = 1.0.65
PACKAGEDIR = $(PACKAGE)-$(VERSION)
PACKAGETGZ = $(PACKAGE)-$(VERSION).tar.gz
LIBVER = 9
LIBREV = 11
FORMATVER = 1.0

# Targets
#HEADERFILES = tcutil.h tchdb.h tcbdb.h tcfdb.h tctdb.h tcadb.h ejdb.h ejdb_private.h bson.h myconf.h
#LIBRARYFILES = libtcejdb.a libtcejdb.so.9.11.0 libtcejdb.so.9 libtcejdb.so
#LIBOBJFILES = tcutil.o tchdb.o tcbdb.o tcfdb.o tctdb.o tcadb.o myconf.o md5.o ejdb.o bson.o numbers.o encoding.o utf8proc.o ejdbutl.o
#COMMANDFILES = tcutest tcumttest tcucodec tchtest tchmttest tchmgr tcbtest tcbmttest tcbmgr tcftest tcfmttest tcfmgr tcttest tctmttest tctmgr tcatest tcamttest tcamgr
#CGIFILES = tcawmgr.cgi
#MAN1FILES =
#MAN3FILES = libtcejdb.3
#DOCUMENTFILES = COPYING Changelog
#PCFILES = tcejdb.pc

# Install destinations
prefix = /usr/local
exec_prefix = ${prefix}
datarootdir = ${prefix}/share
INCLUDEDIR = ${prefix}/include
LIBDIR = ${exec_prefix}/lib
BINDIR = ${exec_prefix}/bin
LIBEXECDIR = ${exec_prefix}/libexec
DATADIR = ${datarootdir}/$(PACKAGE)
MAN1DIR = ${datarootdir}/man/man1
MAN3DIR = ${datarootdir}/man/man3
PCDIR = ${exec_prefix}/lib/pkgconfig

SRCDIR = ./src/cpp/
DESTDIR =

# Building configuration
CC = gcc
CPPFLAGS = -I. -I../tcejdb -I/usr/lib/jvm/java-6-oracle/include -I/usr/lib/jvm/java-6-oracle/include/linux
CFLAGS =  -std=c99 -Wall -fPIC -fsigned-char -O2
LDFLAGS = -L. -L../tcejdb
CMDLDFLAGS =
LIBS = -lz -lrt -lpthread -lm -lc -shared -ltcejdb
# LDENV = LD_RUN_PATH=/lib:/usr/lib:$(LIBDIR):$(HOME)/lib:/usr/local/lib:$(LIBDIR):.
RUNENV = LD_LIBRARY_PATH=.
POSTCMD = true


all: jejdb

tcejdb:
	$(MAKE) -C ../ all

jejdb:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(DESTDIR)libjejdb.so $(SRCDIR)jejdb.c  $(LIBS)

clean:
	- $(MAKE) -C . clean

.PHONY: all tcejdb jni-jejdb clean
