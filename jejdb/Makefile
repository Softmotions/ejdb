#================================================================
# Setting Variables
#================================================================


SHELL = /bin/bash

# Package information
PACKAGE = jejdb
VERSION = 1.0.1
LIBVER = 9
LIBREV = 11
FORMATVER = 1.0

# Targets
LIBRARYFILES =  libjejdb.so.9.11.0 libjejdb.so.9 libjejdb.so


# Install destinations
prefix = /usr
exec_prefix = ${prefix}
LIBDIR = ${exec_prefix}/lib
DESTDIR =


# Directories
SRCDIR = ./src/cpp/

# Building configuration
CC = gcc
CPPFLAGS = -I. -I. -I../tcejdb -I/usr/lib/jvm/java-6-oracle//include -I/usr/lib/jvm/java-6-oracle//include/linux -I$(INCLUDEDIR) -I/home/tve/include -I/usr/local/include -D_UNICODE -DNDEBUG -D_GNU_SOURCE=1 -D_REENTRANT -D__EXTENSIONS__
CFLAGS =  -std=c99 -Wall -fPIC -fsigned-char -O2
LDFLAGS = -L. -L$(LIBDIR) -L/home/tve/lib -L/usr/local/lib
LIBS = -lz -ltcejdb -lrt -lpthread -lm -lc 


#================================================================
# Actions
#================================================================

all: $(LIBRARYFILES) jejdb tests
	@printf '\n'
	@printf '#================================================================\n'
	@printf '# Ready to install.\n'
	@printf '#================================================================\n'

compile-native : $(LIBRARYFILES)

jejdb:
	ant build

tests:
	ant build.and.test

doc:
	ant build.doc

install:
	install -d $(DESTDIR)$(LIBDIR)
	cp -Rf $(LIBRARYFILES) $(DESTDIR)$(LIBDIR)
	chmod -R 755 $(DESTDIR)$(LIBDIR)
	@printf '\n'
	@printf '#================================================================\n'
	@printf '# Java Bindinig binaries installed to %s.\n' "$(DESTDIR)$(LIBDIR)"
	@printf '# Ensure that LD_LIBRARY_PATH environment variable contains "%s".\n' "$(DESTDIR)$(LIBDIR)"
	@printf '#================================================================\n'
	@printf '# Thanks for using Java Binding of Tokyo Cabinet EJDB edition.\n'
	@printf '#================================================================\n'


clean:
	rm -rf $(LIBRARYFILES)
	ant clean


#================================================================
# Building binaries
#================================================================


libjejdb.so.$(LIBVER).$(LIBREV).0 :
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared $(LDFLAGS) -o $@ $(SRCDIR)jejdb.c  $(LIBS)

libjejdb.so.$(LIBVER) : libjejdb.so.$(LIBVER).$(LIBREV).0
	ln -f -s libjejdb.so.$(LIBVER).$(LIBREV).0 $@


libjejdb.so : libjejdb.so.$(LIBVER).$(LIBREV).0
	ln -f -s libjejdb.so.$(LIBVER).$(LIBREV).0 $@
