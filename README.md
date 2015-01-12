[![EJDB](https://raw.github.com/Softmotions/ejdb-pub/master/misc/ejdblogo3.png)](http://ejdb.org)


NOTE
===================================
Despite a long pause in the project activity the team came back.  We are working on the next version of EJDB engine (EJDB2). We will make some maintenance tasks related to the current version of EJDB but no more big changes in the stable codebase.


Embedded JSON Database engine
====================================

It aims to be a fast [MongoDB](http://mongodb.org)-like library **which can be embedded into C/C++, .Net, NodeJS, Python, Lua, Go, Java and Ruby applications under terms of LGPL license.**

EJDB is the C library based on modified version of [Tokyo Cabinet](http://fallabs.com/tokyocabinet/).

JSON representation of queries and data implemented with API based on [C BSON](https://github.com/mongodb/mongo-c-driver/tree/master/src/)

News
===============================
* `2013-09-20` **[EJDB Matlab binding from Kota Yamaguchi] (https://github.com/kyamagu/matlab-ejdb)**
* `2013-09-10` **[v1.1.24 Fixed incorrect $set behaviour] (https://github.com/Softmotions/ejdb/wiki/V1.1.24)**
* `2013-08-19` **[v1.1.19 Added support the long awaited mongodb update positional operator] (https://github.com/Softmotions/ejdb/wiki/V1.1.19)**
* `2013-08-11` **[v1.1.18 Added support for mongodb $ projection] (https://github.com/Softmotions/ejdb/wiki/V1.1.18)**
* `2013-08-08` **[v1.1.17 Now supported $and & $or mongodb operators] (https://github.com/Softmotions/ejdb/issues/81)**
* `2013-07-15` **[Google Go binding] (https://github.com/mkilling/goejdb)**
* `2013-06-23` **[C# .Net binding] (https://github.com/Softmotions/ejdb-csharp)**
* `2013-06-02` **[Adobe Air Native Extension (ANE) for EJDB (Thanks to @thejustinwalsh)] (https://github.com/thejustinwalsh/airejdb)**
* `2013-05-29` **[EJDB Python 2.7.x binding available](https://github.com/Softmotions/ejdb-python)**
* `2013-05-06` **[Ruby binding available](https://github.com/Softmotions/ejdb-ruby)**
* `2013-05-02` **[NodeJS win32 module available](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md#ejdb-nodejs-module-installation)**
* `2013-04-25` **[EJDB and TokyoCabinet API ported to Windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)**
* `2013-04-03` **[Java API binding available](https://github.com/Softmotions/ejdb-java)**
* `2013-03-20` **[Lua binding available](https://github.com/Softmotions/ejdb-lua/)**
* `2013-02-15` **[EJDB Python3 binding available](https://github.com/Softmotions/ejdb-python)**
* `2013-02-07` **[Debian packages provided](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation)**
* `2013-01-22` **[Collection joins now supported](https://github.com/Softmotions/ejdb/wiki/Collection-joins)**

Features
================================
* LGPL license allows you to embed this library into proprietary software.
* [EJDB and TokyoCabinet API ported to Windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)
* MongoDB-like queries and overall philosophy.
* [Collection joins](https://github.com/Softmotions/ejdb/wiki/Collection-joins)
* Collection level write locking.
* Collection level transactions.
* Node.js/Python/Lua/Java/Ruby/.Net/Go bindings
* [Adobe Air Native Extension (ANE) for EJDB] (https://github.com/thejustinwalsh/airejdb)
* [Pike language binding] (https://github.com/hww3/pike_modules-ejdb)

Documentation
================================

* **[The brief intro to the EJDB](http://blog.abigopal.com/post/51616277039/ejdb)**
* **[EJDB Command line interface](https://github.com/Softmotions/ejdb/wiki/EJDB-Command-line-interface)**
* **[EJDB C Library](#ejdb-c-library)**
    * [Building & Installation](#building--installation)
    * [Samples](#ejdb-c-samples)
    * [C API](#c-api)
* **[Data Import/Export](https://github.com/Softmotions/ejdb/wiki/Data-Import-Export)**
* **[Collection joins](https://github.com/Softmotions/ejdb/wiki/Collection-joins)**
* **[Development FAQ](https://github.com/Softmotions/ejdb/wiki/Development-FAQ)**
* **Bindings**
    * **[C# .Net] (https://github.com/Softmotions/ejdb-csharp)**
    * **[NodeJS] (https://github.com/Softmotions/ejdb-node)**
    * **[Python 2.7/3.x](https://github.com/Softmotions/ejdb-python)**
    * **[Lua](https://github.com/Softmotions/ejdb-lua/)**
    * **[Java](https://github.com/Softmotions/ejdb-java)**
    * **[Ruby](https://github.com/Softmotions/ejdb-ruby)**
    * **[Objective-C](https://github.com/johnnyd/EJDBKit)**
    * **[Go](https://github.com/mkilling/goejdb/)**
    * **[Pike language] (https://github.com/hww3/pike_modules-ejdb)**
    * **[Adobe Air] (https://github.com/thejustinwalsh/airejdb)**
    * **[Matlab] (https://github.com/kyamagu/matlab-ejdb)**



Community
================================
* **[ejdblab@twitter.com](https://twitter.com/ejdblab)**
* **We use [EJDB Google group](http://groups.google.com/group/ejdb) as our mailing list.**
* [Projects using EJDB](https://github.com/Softmotions/ejdb/wiki/Projects-using-EJDB)

EJDB NodeJS
=================================

One snippet intro
---------------------------------

```JavaScript
var EJDB = require("ejdb");
//Open zoo DB
var jb = EJDB.open("zoo", EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);

var parrot1 = {
    "name" : "Grenny",
    "type" : "African Grey",
    "male" : true,
    "age" : 1,
    "birthdate" : new Date(),
    "likes" : ["green color", "night", "toys"],
    "extra1" : null
};
var parrot2 = {
    "name" : "Bounty",
    "type" : "Cockatoo",
    "male" : false,
    "age" : 15,
    "birthdate" : new Date(),
    "likes" : ["sugar cane"]
};

jb.save("parrots", [parrot1, parrot2], function(err, oids) {
    if (err) {
        console.error(err);
        return;
    }
    console.log("Grenny OID: " + parrot1["_id"]);
    console.log("Bounty OID: " + parrot2["_id"]);

    jb.find("parrots",
            {"likes" : "toys"},
            {"$orderby" : {"name" : 1}},
            function(err, cursor, count) {
                if (err) {
                    console.error(err);
                    return;
                }
                console.log("Found " + count + " parrots");
                while (cursor.next()) {
                    console.log(cursor.field("name") + " likes toys!");
                }
                cursor.close(); //It's not mandatory to close cursor explicitly
                jb.close(); //Close the database
            });
});
```
**[EJDB NodeJS binding page](https://github.com/Softmotions/ejdb-node)**

EJDB Python
==================================

One snippet intro
---------------------------------

```python
import pyejdb
from datetime import datetime

#Open database
ejdb = pyejdb.EJDB("zoo", pyejdb.DEFAULT_OPEN_MODE | pyejdb.JBOTRUNC)

parrot1 = {
    "name": "Grenny",
    "type": "African Grey",
    "male": True,
    "age": 1,
    "birthdate": datetime.utcnow(),
    "likes": ["green color", "night", "toys"],
    "extra1": None
}
parrot2 = {
    "name": "Bounty",
    "type": "Cockatoo",
    "male": False,
    "age": 15,
    "birthdate": datetime.utcnow(),
    "likes": ["sugar cane"],
    "extra1": None
}
ejdb.save("parrots2", parrot1, parrot2)

with ejdb.find("parrots2", {"likes" : "toys"},
          hints={"$orderby" : [("name", 1)]}) as cur:
    print("found %s parrots" % len(cur))
    for p in cur:
        print("%s likes toys!" % p["name"])

ejdb.close()
```
**[EJDB Python 2.7/3.x binding page](https://github.com/Softmotions/ejdb-python)**

EJDB Lua
==================================

One snippet intro
---------------------------------

```lua
local ejdb = require("ejdb")
local inspect = require("ejdb.inspect")
local Q = ejdb.Q

-- Used modes:
-- 'r' - read
-- 'w' - write
-- 'c' - create db if not exists
-- 't' - truncate existing db
local db = ejdb.open("zoo", "rwct")

-- Unordered lua table
local parrot1 = {
  name = "Grenny",
  type = "African Grey",
  male = true,
  age = 1,
  birthhdate = ejdb.toDateNow(),
  likes = { "green color", "night", "toys" },
  extra1 = ejdb.toNull()
}

-- Preserve order of BSON keys
local parrot2 = Q();
parrot2:KV("name", "Bounty"):KV("type", "Cockatoo"):KV("male", false)
parrot2:KV("age", 15):KV("birthdate",
  ejdb.toDate({ year = 2013, month = 1, day = 1, hour = 0, sec = 1 }))
parrot2:KV("likes", { "sugar cane" }):KV("extra1", ejdb.toNull())

--IF YOU WANT SOME DATA INSPECTIONS:
--print(ejdb.print_bson(parrot2:toBSON()))
--local obj = ejdb.from_bson(parrot2:toBSON())
--print(inspect(obj));

db:save("parrots2", parrot1)
db:save("parrots2", parrot2)

-- Below two equivalent queries:
-- Q1
local res, count, log =
db:find("parrots2", Q("likes", "toys"):OrderBy("name asc", "age desc"))
for i = 1, #res do -- iterate one
  local ob = res:object(i)
  print("" .. ob["name"] .. " likes toys #1")
end

-- Q2
local res, count, log =
db:find("parrots2", Q():F("likes"):Eq("toys"):OrderBy({ name = 1 }, { age = -1 }))
for i = 1, #res do -- iterate one
  print("" .. res:field(i, "name") .. " likes toys #2")
end

-- Second way to iterate
for vobj, idx in res() do
  print("" .. vobj["name"] .. " likes toys #3")
end

db:close()
```
**[EJDB Lua binding page](https://github.com/Softmotions/ejdb-lua)**

EJDB Go
==================================

One snippet intro
-----------------------------------

```go
package ejdbtutorial

import (
    "fmt"
    "github.com/mkilling/goejdb"
    "labix.org/v2/mgo/bson"
    "os"
)

func main() {
    // Create a new database file and open it
    jb, err := goejdb.Open("addressbook", JBOWRITER | JBOCREAT | JBOTRUNC)
    if err != nil {
        os.Exit(1)
    }
    // Get or create collection 'contacts'
    coll, _ := jb.CreateColl("contacts", nil)

    // Insert one record:
    // JSON: {'name' : 'Bruce', 'phone' : '333-222-333', 'age' : 58}
    rec := map[string]interface{} {"name" : "Bruce", "phone" : "333-222-333", "age" : 58}
    bsrec, _ := bson.Marshal(rec)
    coll.SaveBson(bsrec)
    fmt.Printf("\nSaved Bruce")

    // Now execute query
    res, _ := coll.Find(`{"name" : {"$begin" : "Bru"}}`) // Name starts with 'Bru' string
    fmt.Printf("\n\nRecords found: %d\n", len(res))

    // Now print the result set records
    for _, bs := range res {
        var m map[string]interface{}
        bson.Unmarshal(bs, &m)
        fmt.Println(m)
    }

    // Close database
    jb.Close()
}
```
**[EJDB Go binding page](https://github.com/mkilling/goejdb)**


EJDB Ruby
==================================

One snippet intro
---------------------------------

```Ruby
require "rbejdb"

#Open zoo DB
jb = EJDB.open("zoo", EJDB::DEFAULT_OPEN_MODE | EJDB::JBOTRUNC)

parrot1 = {
    "name" => "Grenny",
    "type" => "African Grey",
    "male" => true,
    "age" => 1,
    "birthdate" => Time.now,
    "likes" => ["green color", "night", "toys"],
    "extra1" => nil
}
parrot2 = {
    "name" => "Bounty",
    "type" => "Cockatoo",
    "male" => false,
    "age" => 15,
    "birthdate" => Time.now,
    "likes" => ["sugar cane"],
    "extra1" => nil
}

jb.save("parrots", parrot1, parrot2)
puts "Grenny OID: #{parrot1["_id"]}"
puts "Bounty OID: #{parrot2["_id"]}"

results = jb.find("parrots", {"likes" => "toys"}, {"$orderby" => {"name" => 1}})

puts "Found #{results.count} parrots"

results.each { |res|
  puts "#{res['name']} likes toys!"
}

results.close #It's not mandatory to close cursor explicitly
jb.close #Close the database

```
**[EJDB Ruby binding page](https://github.com/Softmotions/ejdb-ruby)**


EJDB Adobe AIR
==================================

One snippet intro
---------------------------------

```as3
// Open the zoo DB
var db:EJDBDatabase = EJDB.open("zoo", EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC) as EJDBDatabase;

var parrot1:Object = {
	"name" : "Grenny",
	"type" : "African Grey",
	"male" : true,
	"age" : 1,
	"birthdate" : new Date(),
	"likes" : ["green color", "night", "toys"],
	"extra1" : null
};
var parrot2:Object = {
	"name" : "Bounty",
	"type" : "Cockatoo",
	"male" : false,
	"age" : 15,
	"birthdate" : new Date(),
	"likes" : ["sugar cane"]
};

var oids:Array = db.save("parrots", [parrot1, parrot2]);
trace("Grenny OID: " + parrot1._id);
trace("Bounty OID: " + parrot2._id);

var cursor:EJDBCursor = db.find("parrots",
	{"likes" : "toys"},
	[],
	{"$orderby" : {"name" : 1}}
);

trace("Found " + cursor.length + " parrots");
while (cursor.next()) {
	trace(cursor.field("name") + " likes toys!");
}

cursor.close(); // It IS mandatory to close cursor explicitly to free up resources
db.close(); // Close the database
```
**[Adobe Air Native Extension (ANE) for EJDB] (https://github.com/thejustinwalsh/airejdb)**

EJDB C Library
==================================

One snippet intro
-----------------------------------

```C
#include <tcejdb/ejdb.h>

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

You can save this code in `csnippet.c` And build:

```sh
gcc -std=c99 -Wall -pedantic  -c -o csnippet.o csnippet.c
gcc -o csnippet csnippet.o -ltcejdb
```

Building & Installation
--------------------------------
 * [Installation on windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)
 * [Installation on Debian/Ubuntu](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation)
 * Installation on OS X 10.7+ using [Homebrew](http://brew.sh/): `brew install ejdb`

Manual installation
-------------------------------

### Prerequisites
**System libraries:**

* gcc
* zlib-dev

### Build and install

```sh
   cd ./tcejdb
   ./configure --prefix=<installation prefix> && make && make check
   make install
```
* library name: **tcejdb** (with pkgconfig)
* main include header: ```<tcejdb/ejdb.h>```

C API
---------------------------------
EJDB API presented in **[ejdb.h](https://github.com/Softmotions/ejdb/blob/master/tcejdb/ejdb.h)** C header file.

JSON processing API: **[bson.h](https://github.com/Softmotions/ejdb/blob/master/tcejdb/bson.h)**

Queries
---------------------------------

~~~~~~
/**
 * Create query object.
 * Sucessfully created queries must be destroyed with ejdbquerydel().
 *
 * EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.
 *
 *  - Supported queries:
 *      - Simple matching of String OR Number OR Array value:
 *          -   {'fpath' : 'val', ...}
 *      - $not Negate operation.
 *          -   {'fpath' : {'$not' : val}} //Field not equal to val
 *          -   {'fpath' : {'$not' : {'$begin' : prefix}}} //Field not begins with val
 *      - $begin String starts with prefix
 *          -   {'fpath' : {'$begin' : prefix}}
 *      - $gt, $gte (>, >=) and $lt, $lte for number types:
 *          -   {'fpath' : {'$gt' : number}, ...}
 *      - $bt Between for number types:
 *          -   {'fpath' : {'$bt' : [num1, num2]}}
 *      - $in String OR Number OR Array val matches to value in specified array:
 *          -   {'fpath' : {'$in' : [val1, val2, val3]}}
 *      - $nin - Not IN
 *      - $strand String tokens OR String array val matches all tokens in specified array:
 *          -   {'fpath' : {'$strand' : [val1, val2, val3]}}
 *      - $stror String tokens OR String array val matches any token in specified array:
 *          -   {'fpath' : {'$stror' : [val1, val2, val3]}}
 *      - $exists Field existence matching:
 *          -   {'fpath' : {'$exists' : true|false}}
 *      - $icase Case insensitive string matching:
 *          -    {'fpath' : {'$icase' : 'val1'}} //icase matching
 *          Ignore case matching with '$in' operation:
 *          -    {'name' : {'$icase' : {'$in' : ['tHéâtre - театр', 'heLLo WorlD']}}}
 *          For case insensitive matching you can create special index of type: `JBIDXISTR`
 *     - $elemMatch The $elemMatch operator matches more than one component within an array element.
 *          -  { array: { $elemMatch: { value1 : 1, value2 : { $gt: 1 } } } }
 *          Restriction: only one $elemMatch allowed in context of one array field.
 *      - $and, $or joining:
 *          -   {..., $and : [subq1, subq2, ...] }
 *          -   {..., $or  : [subq1, subq2, ...] }
 *          Example: {z : 33, $and : [ {$or : [{a : 1}, {b : 2}]}, {$or : [{c : 5}, {d : 7}]} ] }
 *
 *      - Mongodb $(projection) operator supported. (http://docs.mongodb.org/manual/reference/projection/positional/#proj._S_)
 *      - Mongodb positional $ update operator supported. (http://docs.mongodb.org/manual/reference/operator/positional/)
 *
 *
 *  - Queries can be used to update records:
 *       $set Field set operation.
 *           - {.., '$set' : {'field1' : val1, 'fieldN' : valN}}
 *       $upsert Atomic upsert. If matching records are found it will be '$set' operation,
 *              otherwise new record will be inserted with fields specified by argment object.
 *           - {.., '$upsert' : {'field1' : val1, 'fieldN' : valN}}
 *       $inc Increment operation. Only number types are supported.
 *           - {.., '$inc' : {'field1' : number, ...,  'field1' : number}
 *       $dropall In-place record removal operation.
 *           - {.., '$dropall' : true}
 *       $addToSet Atomically adds value to the array only if its not in the array already.
 *                    If containing array is missing it will be created.
 *           - {.., '$addToSet' : {'fpath' : val1, 'fpathN' : valN, ...}}
 *       $addToSetAll Batch version if $addToSet
 *           - {.., '$addToSetAll' : {'fpath' : [array of values to add], ...}}
 *       $pull Atomically removes all occurrences of value from field, if field is an array.
 *           - {.., '$pull' : {'fpath' : val1, 'fpathN' : valN, ...}}
 *       $pullAll Batch version of $pull
 *           - {.., '$pullAll' : {'fpath' : [array of values to remove], ...}}
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
 *      - $fields Set subset of fetched fields
            If a field presented in $orderby clause it will be forced to include in resulting records.
 *          Example:
 *          hints:    {
 *                      "$orderby" : { //ORDER BY field1 ASC, field2 DESC
 *                          "field1" : 1,
 *                          "field2" : -1
 *                      },
 *                      "$fields" : { //SELECT ONLY {_id, field1, field2}
 *                          "field1" : 1,
 *                          "field2" : 1
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
~~~~~~

EJDB C Samples
------------------------------------
You can find some code samples in:

* [tcejdb/samples](https://github.com/Softmotions/ejdb/tree/master/tcejdb/samples)
* [tcejdb/testejdb](https://github.com/Softmotions/ejdb/tree/master/tcejdb/testejdb)

Basic EJDB architecture
------------------------------------
**EJDB database files structure**

~~~~~~
.
├── <dbname>
├── <dbname>_<collection1>
├── ...
├── <dbname>_<collectionN>
└── <dbname>_<collectionN>_<fieldpath>.<index ext>
~~~~~~

Where

* ```<dbname>``` - name of database. It is metadata DB.
* ```<collectionN>``` - name of collection. Collection database.
* ```<fieldpath>``` - JSON field path used in index
* ```<index ext>``` - Collection index extension:
    * ```.lex``` String index
    * ```.dec``` Number index
    * ```.tok``` Array index

Limitations
------------------------------------
* One ejdb database can handle up to 1024 collections.
* Indexes for objects in nested arrays currently not supported (#37)

TODO
------------------------------------
* Collect collection index statistic
