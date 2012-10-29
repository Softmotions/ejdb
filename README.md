

EJDB - Embedded JSON Database engine
====================================

The main goal of this project to create the fast [MongoDB](http://mongodb.org)-like database engine library **which can be embed
into C/C++ applications under terms of LGPL license.**

EJDB is the C library based on modified version of [Tokyo Cabinet](http://fallabs.com/tokyocabinet/).

JSON representation of queries and data implemented with API based on [C BSON](https://github.com/mongodb/mongo-c-driver/tree/master/src/)


One snippet intro
-----------------------------------

```C
#include <tcejdb/ejdb.h>

static EJDB *jb;

int main() {
    jb = ejdbnew();
    if (!ejdbopen(jb, "addressbook", JDBOWRITER | JDBOCREAT | JDBOTRUNC)) {
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
    TCLIST *res = ejdbqrysearch(coll, q1, &count, 0, NULL);
    fprintf(stderr, "\n\nRecords found: %d\n", count);

    //Now print the result set records
    for (int i = 0; i < TCLISTNUM(res); ++i) {
        void *bsdata = TCLISTVALPTR(res, i);
        bson_print_raw(stderr, bsdata, 0);
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

Features
================================
* LGPL license allows you to embed this library into proprietary software.
* MongoDB-like queries and overall philosophy.
* Collection level write locking.
* Collection level transactions.
* String token matching queries: ```$stror``` ```$strand```


Building & Installation
================================

Prerequisites
--------------------------------
**Libraries:**

* cunit
* zlib
* bzip2

On Debian/Ubuntu linux you can install it as follows:

```sh
   sudo apt-get install libcunit1 libcunit1-dev libbz2-1.0 libbz2-dev
```

Building
--------------------------------
```sh
   ./configure --prefix=<installation prefix> && make && make check
   make install
```
* library name: **tcejdb** (with pkgconfig)
* main include header: ```<tcejdb/ejdb.h>```

Usage
===============================

Basic EJDB architecture
-------------------------------
**EJDB database files structure**

```
.
├── <dbname>
├── <dbname>_<collection1>
├── ...
├── <dbname>_<collectionN>
└── <dbname>_<collectionN>_<fieldpath>.<index ext>
```

Where

* ```<dbname>``` - name of database. It is metadata DB.
* ```<collectionN>``` - name of collection. Collection database.
* ```<fieldpath>``` - JSON field path used in index
* ```<index ext>``` - Collection index extension:
    * ```.lex``` String index
    * ```.dec``` Number index
    * ```.tok``` Array index

API
---------------------------------
EJDB API presented in **ejdb.h** C header file.

JSON processing API: **bson.h**

Queries
---------------------------------

```
/**
 * Create query object.
 * Sucessfully created queries must be destroyed with ejdbquerydel().
 *
 * EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.
 *
 *  - Supported queries:
 *      - Simple matching of String OR Number OR Array value:
 *          -   {'bson.field.path' : 'val', ...}
 *      - $not Negate operation.
 *          -   {'json.field.path' : {'$not' : val}} //Field not equal to val
 *          -   {'json.field.path' : {'$not' : {'$begin' : prefix}}} //Field not begins with val
 *      - $begin String starts with prefix
 *          -   {'json.field.path' : {'$begin' : prefix}}
 *      - $gt, $gte (>, >=) and $lt, $lte for number types:
 *          -   {'json.field.path' : {'$gt' : number}, ...}
 *      - $bt Between for number types:
 *          -   {'json.field.path' : {'$bt' : [num1, num2]}}
 *      - $in String OR Number OR Array val matches to value in specified array:
 *          -   {'json.field.path' : {'$in' : [val1, val2, val3]}}
 *      - $nin - Not IN
 *      - $strand String tokens OR String array val matches all tokens in specified array:
 *          -   {'json.field.path' : {'$strand' : [val1, val2, val3]}}
 *      - $stror String tokens OR String array val matches any token in specified array:
 *          -   {'json.field.path' : {'$stror' : [val1, val2, val3]}}
 *
 *  NOTE: Negate operations: $not and $nin not using indexes
 *  so they can be slow in comparison to other matching operations.
 *
 *  NOTE: Only one index can be used in search query operation.
 *
 *  QUERY HINTS (specified by `hints` argument):
 *      - $max Maximum number in the result set
 *      - $skip Number of skipped results in the result set
 *      - $orderby Sorting order of query fields.
 *          Eg: ORDER BY field1 ASC, field2 DESC
 *          hints:    {
 *                      "$orderby" : {
 *                          "field1" : 1,
 *                          "field2" : -1
 *                      }
 *                    }
 *
 * Many query examples can be found in `testejdb/t2.c` test case.
 *
 * @param EJDB database handle.
 * @param qobj Main BSON query object.
 * @param orqobjs Array of additional OR query objects (joined with OR predicate).
 * @param orqobjsnum Number of OR query objects.
 * @param hints BSON object with query hints.
 * @return On success return query handle. On error returns NULL.
 */
EJDB_EXPORT EJQ* ejdbcreatequery(EJDB *jb, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints);
```

Examples
------------------------------------
You can find some code samples in:

* /samples
* /testejdb test cases

Limitations/TODOs
-----------------------------------
* Case insensitive string indexes
* Collect collection index statistic
* Windows port

