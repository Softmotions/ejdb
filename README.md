[![EJDB](http://ejdb.org/_images/ejdblogo3.png)](http://ejdb.org)

[EJDB v1.2.12 released (2017-21-03)](https://github.com/Softmotions/ejdb/releases/tag/v1.2.12)

Embedded JSON Database engine C library (libejdb)
=================================================

[![Join the chat at https://gitter.im/Softmotions/ejdb](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/Softmotions/ejdb?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

**See http://ejdb.org**

It aims to be a fast [MongoDB](http://mongodb.org)-like library 
**which can be embedded into C/C++, .Net, NodeJS, Python, Lua, Go, Java 
and Ruby applications under terms of LGPL license.**

EJDB is the C library based on modified 
version of [Tokyo Cabinet](http://fallabs.com/tokyocabinet/).

JSON representation of queries and data implemented with API based on 
[C BSON](https://github.com/mongodb/mongo-c-driver/tree/master/src/) 
     
**[Roadmap and thoughts for the next major EJDB 2.0 release](https://github.com/Softmotions/ejdb/issues/183)**     
     
Features
========
* LGPL license allows you to embed this library into proprietary software
* Simple C libejdb library can be easily embedded into any software
* Node.js/Python/Lua/Java/Ruby/.Net/Go/Pike/Mathlab/AdobeAIR bindings available
* MongoDB-like queries and overall philosophy.
* [Collection joins](https://github.com/Softmotions/ejdb/wiki/Collection-joins)

Usage
=====

One snippet intro
-----------------------------------

```C
#include <ejdb/ejdb.h>

static EJDB *jb;

int main() {
    jb = ejdbnew();
    if (!ejdbopen(jb, "addressbook", JBOWRITER | JBOCREAT | JBOTRUNC)) {
        return 1;
    }
    
    //Get or create collection 'contacts'
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);

    bson bsrec;
    bson_oid_t oid;

    //Insert one record:
    //JSON: {'name' : 'Bruce', 'phone' : '333-222-333', 'age' : 58}
    bson_init(&bsrec);
    bson_append_string(&bsrec, "name", "Bruce");
    bson_append_string(&bsrec, "phone", "333-222-333");
    bson_append_int(&bsrec, "age", 58);
    bson_finish(&bsrec);
    
    //Save BSON
    ejdbsavebson(coll, &bsrec, &oid);
    fprintf(stderr, "\nSaved Bruce");
    bson_destroy(&bsrec);

    //Now execute query
    //QUERY: {'name' : {'$begin' : 'Bru'}} //Name starts with 'Bru' string
    bson bq1;
    bson_init_as_query(&bq1);
    bson_append_start_object(&bq1, "name");
    bson_append_string(&bq1, "$begin", "Bru");
    bson_append_finish_object(&bq1);
    bson_finish(&bq1);

    EJQ *q1 = ejdbcreatequery(jb, &bq1, NULL, 0, NULL);

    uint32_t count;
    TCLIST *res = ejdbqryexecute(coll, q1, &count, 0, NULL);
    fprintf(stderr, "\n\nRecords found: %d\n", count);

    //Now print the result set records
    for (int i = 0; i < TCLISTNUM(res); ++i) {
        void *bsdata = TCLISTVALPTR(res, i);
        bson_print_raw(bsdata, 0);
    }
    fprintf(stderr, "\n");

    //Dispose result set
    tclistdel(res);

    //Dispose query
    ejdbquerydel(q1);
    bson_destroy(&bq1);

    //Close database
    ejdbclose(jb);
    ejdbdel(jb);
    return 0;
}
```

Assuming libejdb installed on your system you can place the code above in `csnippet.c` and build program:

```sh
gcc -std=c99 -Wall -pedantic  -c -o csnippet.o csnippet.c
gcc -o csnippet csnippet.o -lejdb
```
     
C API
=====

EJDB API exposed in **[ejdb.h](https://github.com/Softmotions/ejdb/blob/master/src/ejdb/ejdb.h)** C header file.
JSON processing API: **[bson.h](https://github.com/Softmotions/ejdb/blob/master/src/bson/bson.h)**


Building
========

Prerequisites
-------------

 * git  
 * GNU make 
 * cmake >= 2.8.12
 * gcc >= 4.7 or clang >= 3.4 C compiller
 * zlib-dev 
 
Make
----

```sh
git clone https://github.com/Softmotions/ejdb.git
cd ejdb
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make 
make install
# Or you can create tgz package:
make package
```

### CMake basic build(-D) options

```
// Build ejdb sample projects
BUILD_SAMPLES:BOOL=ON

// Build shared libraries
BUILD_SHARED_LIBS:BOOL=ON

// Build test cases
BUILD_TESTS:BOOL=OFF

// Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo.
CMAKE_BUILD_TYPE:STRING=Release

// Install path prefix, prepended onto install directories.
CMAKE_INSTALL_PREFIX:PATH=/usr/local

// Enable PPA package build
ENABLE_PPA:BOOL=OFF

// Build .deb instalation packages
PACKAGE_DEB:BOOL=OFF

// Build .tgz package archive
PACKAGE_TGZ:BOOL=ON

// Upload debian packages to the launchpad ppa repository
UPLOAD_PPA:BOOL=OFF
```

### Building Windows libs

You need to checkout the [MXE cross build environment](http://mxe.cc)
Then create/edit MXE settings file:

```sh
cd <mxe checkout dir>
nano ./settings.mk
```

Save the following content in `settings.mk`:

```
JOBS := 1
MXE_TARGETS := x86_64-w64-mingw32.static i686-w64-mingw32.static
LOCAL_PKG_LIST := winpthreads pcre zlib lzo bzip2 cunit
.DEFAULT local-pkg-list:
local-pkg-list: $(LOCAL_PKG_LIST)
```

Build MXE packages:

```sh
 cd <mxe checkout dir>
 make
```

Build libejdb windows binaries:

```sh
export MXE_HOME=<mxe checkout dir>
cd <ejdb checkout dir>
mkdir build-win32
cd build-wind32
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../win64-tc.cmake ..
make package
```

EJDB binary package installation
===================================
     
Ubuntu
------

```sh
sudo add-apt-repository ppa:adamansky/ejdb
sudo apt-get update
sudo apt-get install ejdb ejdb-dbg
``` 

