[![EJDB](https://raw.github.com/Softmotions/ejdb/master/misc/ejdblogo3.png)](http://ejdb.org)


Embedded JSON Database engine
====================================

It aims to be a fast [MongoDB](http://mongodb.org)-like library **which can be embedded into C/C++/NodeJS/Python/Lua/Java/Ruby applications under terms of LGPL license.**

EJDB is the C library based on modified version of [Tokyo Cabinet](http://fallabs.com/tokyocabinet/).

JSON representation of queries and data implemented with API based on [C BSON](https://github.com/mongodb/mongo-c-driver/tree/master/src/)

News
===============================
* `2013-05-29` **[EJDB Python 2.7.x binding available](https://github.com/Softmotions/ejdb/blob/master/pyejdb/)**
* `2013-05-06` **[Ruby binding available](https://github.com/Softmotions/ejdb/blob/master/rbejdb/README.md)**
* `2013-05-02` **[NodeJS win32 module available](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md#ejdb-nodejs-module-installation)**
* `2013-04-25` **[EJDB and TokyoCabinet API ported to Windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)**
* `2013-04-03` **[Java API binding available](https://github.com/Softmotions/ejdb/blob/master/jejdb/)**
* `2013-03-20` **[Lua binding available](https://github.com/Softmotions/ejdb/blob/master/luaejdb/)**
* `2013-02-15` **[EJDB Python3 binding available](https://github.com/Softmotions/ejdb/blob/master/pyejdb/)**
* `2013-02-07` **[Debian packages provided](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation)**
* `2013-01-22` **[Collection joins now supported](https://github.com/Softmotions/ejdb/wiki/Collection-joins)**

Features
================================
* LGPL license allows you to embed this library into proprietary software.
* [EJDB and TokyoCabinet API ported to Windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)
* MongoDB-like queries and overall philosophy.
* Collection level write locking.
* Collection level transactions.
* String token matching queries: ```$stror``` ```$strand```
* [Node.js](http://nodejs.org) binding
* [Collection joins](https://github.com/Softmotions/ejdb/wiki/Collection-joins)
* Python/Lua/Java/Ruby bindings


Documentation
================================

* **[EJDB Command line interface](https://github.com/Softmotions/ejdb/wiki/EJDB-Command-line-interface)**
* **[NodeJS binding](#nodejs-binding)**
    * [Installation](#installation)
    * [Samples](#ejdb-nodejs-samples)
    * [NodeJS API](#ejdb-nodejs-api)
* **[Python 2.7/3.x binding](https://github.com/Softmotions/ejdb/blob/master/pyejdb/)**
* **[Lua binding](https://github.com/Softmotions/ejdb/blob/master/luaejdb/)**
* **[Java binding](https://github.com/Softmotions/ejdb/blob/master/jejdb/)**
* **[Ruby binding](https://github.com/Softmotions/ejdb/blob/master/rbejdb/)**
* **[EJDB C Library](#ejdb-c-library)**
    * [Building & Installation](#building--installation)
    * [Samples](#ejdb-c-samples)
    * [C API](#c-api)
* **[Collection joins](https://github.com/Softmotions/ejdb/wiki/Collection-joins)**


Community
================================
* **We use [EJDB Google group](http://groups.google.com/group/ejdb) as our mailing list.**
* [Projects using EJDB](https://github.com/Softmotions/ejdb/wiki/Projects-using-EJDB)

NodeJS binding
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

Installation
--------------------------------
**System libraries:**

* g++
* zlib

On Debian/Ubuntu linux you can install it as follows:

~~~~~~
sudo apt-get install g++ zlib1g zlib1g-dev
~~~~~~

**Installation from node package manager on linux/macos:**

    npm install ejdb

**[Installing EJDB NodeJS module on windows](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md#ejdb-nodejs-module-installation)**


EJDB NodeJS samples
---------------------------------

* [node/samples](https://github.com/Softmotions/ejdb/tree/master/node/samples)
* [node/tests](https://github.com/Softmotions/ejdb/tree/master/node/tests)
* [nwk-ejdb-address-book](https://github.com/Softmotions/nwk-ejdb-address-book)


EJDB NodeJS API
----------------------------------

<a name="open" />
### EJDB.open(dbFile, openMode)

Open database. Return database instance handle object.
<br/>Default open mode: `JBOWRITER | JBOCREAT`.
<br/>This is blocking function.

__Arguments__

 * {String} dbFile Database main file name
 * {Number} `[openMode=JBOWRITER | JBOCREAT]` Bitmast of open modes:
       - `JBOREADER` Open as a reader.
       - `JBOWRITER` Open as a writer.
       - `JBOCREAT` Create if db file not exists
       - `JBOTRUNC` Truncate db.

---------------------------------------

<a name="close" />
### close()

Close database.
<br/>If database was not opened it does nothing.
<br/>This is blocking function.

---------------------------------------

<a name="isOpen"/>
### isOpen()
Check if database in opened state.

---------------------------------------

<a name="ensureCollection"/>
### ensureCollection(cname, copts)

Automatically creates new collection if it does't exists.
Collection options `copts` applied only for newly created collection.
For existing collections `copts` takes no effect.

Collection options (copts):

 * cachedrecords : Max number of cached records in shared memory segment. Default: 0
 * records : Estimated number of records in this collection. Default: 65535.
 * large : Specifies that the size of the database can be larger than 2GB. Default: false
 * compressed : If true collection records will be compressed with DEFLATE compression. Default: false.

<br/>This is blocking function.

__Arguments__

 * {String} cname Name of collection.
 * {Object} `[copts]` Collection options.

---------------------------------------


<a name="dropCollection"/>
### dropCollection(cname, prune, cb)

Drop collection.

Call variations:

    dropCollection(cname)
    dropCollection(cname, cb)
    dropCollection(cname, prune, cb)

__Arguments__

 * {String} cname Name of collection.
 * {Boolean} `[prune=false]` If true the collection data will erased from disk.
 * {Function} `[cb]` Callback args: (error)

---------------------------------------

<a name="save"/>
### save(cname, jsarr, cb)

Save/update specified JSON objects in the collection.
If collection with `cname` does not exists it will be created.

Each persistent object has unique identifier (OID) placed in the `_id` property.
If a saved object does not have  `_id` it will be autogenerated.
To identify and update object it should contains `_id` property.

If callback is not provided this function will be synchronous.

Call variations:

    save(cname, <json object>|<Array of json objects>, [options] [cb])
    save(cname, <json object>|<Array of json objects>, [cb])

NOTE: Field names of passed JSON objects may not contain `$` and `.` characters,
      error condition will be fired in this case.

__Arguments__

 * {String} **cname** Name of collection.
 * {Array|Object} jsarr Signle JSON object or array of JSON objects to save
 * {Function} `[cb]` Callback args: (error, {Array} of OIDs for saved objects)

__Return__

 * {Array} of OIDs of saved objects in synchronous mode otherwise returns {undefined}.

--------------------------------------

<a name="load"/>
### load(cname, oid, cb)

Loads JSON object identified by OID from the collection.
If callback is not provided this function will be synchronous.

__Arguments__

 * {String} cname Name of collection
 * {String} oid Object identifier (OID)
 * {Function} cb  Callback args: (error, obj)
        `obj`:  Retrieved JSON object or NULL if it is not found.

__Return__

 * JSON object or {null} if it is not found in synchronous mode otherwise return {undefined}.

--------------------------------------

<a name="remove"/>
### remove(cname, oid, cb)

Removes JSON object from the collection.
If callback is not provided this function will be synchronous.

__Arguments__

 * {String} cname Name of collection
 * {String} oid Object identifier (OID)
 * {Function} cb  Callback args: (error)


--------------------------------------

<a name="find"/>
### find(cname, qobj, orarr, hints, cb)
Execute query on collection.
EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.

    Supported queries:
      - Simple matching of String OR Number OR Array value:
          -   {'fpath' : 'val', ...}
      - $not Negate operation.
          -   {'fpath' : {'$not' : val}} //Field not equal to val
          -   {'fpath' : {'$not' : {'$begin' : prefix}}} //Field not begins with val
      - $begin String starts with prefix
          -   {'fpath' : {'$begin' : prefix}}
      - $gt, $gte (>, >=) and $lt, $lte for number types:
          -   {'fpath' : {'$gt' : number}, ...}
      - $bt Between for number types:
          -   {'fpath' : {'$bt' : [num1, num2]}}
      - $in String OR Number OR Array val matches to value in specified array:
          -   {'fpath' : {'$in' : [val1, val2, val3]}}
      - $nin - Not IN
      - $strand String tokens OR String array val matches all tokens in specified array:
          -   {'fpath' : {'$strand' : [val1, val2, val3]}}
      - $stror String tokens OR String array val matches any token in specified array:
          -   {'fpath' : {'$stror' : [val1, val2, val3]}}
      - $exists Field existence matching:
          -   {'fpath' : {'$exists' : true|false}}
      - $icase Case insensitive string matching:
          -  {'fpath' : {'$icase' : 'val1'}} //icase matching
          Ignore case matching with '$in' operation:
          -  {'name' : {'$icase' : {'$in' : ['tHéâtre - театр', 'heLLo WorlD']}}}
          For case insensitive matching you can create special type of string index.
      - $elemMatch The $elemMatch operator matches more than one component within an array element.
          -  { array: { $elemMatch: { value1 : 1, value2 : { $gt: 1 } } } }
          Restriction: only one $elemMatch allowed in context of one array field.

      - Queries can be used to update records:

        $set Field set operation.
            - {.., '$set' : {'field1' : val1, 'fieldN' : valN}}
        $upsert Atomic upsert. If matching records are found it will be '$set' operation,
                otherwise new record will be inserted with fields specified by argment object.
           - {.., '$upsert' : {'field1' : val1, 'fieldN' : valN}}
        $inc Increment operation. Only number types are supported.
            - {.., '$inc' : {'field1' : number, ...,  'field1' : number}
        $dropall In-place record removal operation.
            - {.., '$dropall' : true}
        $addToSet Atomically adds value to the array only if its not in the array already.
                    If containing array is missing it will be created.
            - {.., '$addToSet' : {'fpath' : val1, 'fpathN' : valN, ...}}
        $addToSetAll Batch version if $addToSet
            - {.., '$addToSetAll' : {'fpath' : [array of values to add], ...}}
        $pull Atomically removes all occurrences of value from field, if field is an array.
            - {.., '$pull' : {'fpath' : val1, 'fpathN' : valN, ...}}
        $pullAll Batch version of $pull
            - {.., '$pullAll' : {'fpath' : [array of values to remove], ...}}

    NOTE: It is better to execute update queries with `$onlycount=true` hint flag
         or use the special `update()` method to avoid unnecessarily rows fetching.

    NOTE: Negate operations: $not and $nin not using indexes
          so they can be slow in comparison to other matching operations.

    NOTE: Only one index can be used in search query operation.

    NOTE: If callback is not provided this function will be synchronous.

    QUERY HINTS (specified by `hints` argument):
      - $max Maximum number in the result set
      - $skip Number of skipped results in the result set
      - $orderby Sorting order of query fields.
      - $onlycount true|false If `true` only count of matching records will be returned
                              without placing records in result set.
      - $fields Set subset of fetched fields
           If a field presented in $orderby clause it will be forced to include in resulting records.
           Example:
           hints:    {
                       "$orderby" : { //ORDER BY field1 ASC, field2 DESC
                           "field1" : 1,
                           "field2" : -1
                       },
                       "$fields" : { //SELECT ONLY {_id, field1, field2}
                           "field1" : 1,
                           "field2" : 1
                       }
                     }

    Many C API query examples can be found in `tcejdb/testejdb/t2.c` test case.

    To traverse selected records cursor object is used:
      - Cursor#next() Move cursor to the next record and returns true if next record exists.
      - Cursor#hasNext() Returns true if cursor can be placed to the next record.
      - Cursor#field(name) Retrieve value of the specified field of the current JSON object record.
      - Cursor#object() Retrieve whole JSON object with all fields.
      - Cursor#reset() Reset cursor to its initial state.
      - Cursor#length Read-only property: Number of records placed into cursor.
      - Cursor#pos Read/Write property: You can set cursor position: 0 <= pos < length
      - Cursor#close() Closes cursor and free cursor resources. Cursor cant be used in closed state.

    Call variations of find():
       - find(cname, [cb])
       - find(cname, qobj, [cb])
       - find(cname, qobj, hints, [cb])
       - find(cname, qobj, qobjarr, [cb])
       - find(cname, qobj, qobjarr, hints, [cb])

 __Arguments__

 * {String} cname Name of collection
 * {Object} qobj Main JSON query object
 * {Array} `[orarr]` Array of additional OR query objects (joined with OR predicate).
 * {Object} `[hints]` JSON object with query hints.
 * {Function} cb Callback args: (error, cursor, count)
            `cursor`: Cursor object to traverse records
      qobj      `count`:  Total number of selected records

__Return__

 * If callback is provided returns {undefined}
 * If no callback and `$onlycount` hint is set returns count {Number}.
 * If no callback and no `$onlycount` hint returns cursor {Object}.

 --------------------------------------------

<a name="findOne"/>
### findOne(cname, qobj, orarr, hints, cb)
Same as #find() but retrieves only one matching JSON object.

Call variations of findOne():

    findOne(cname, [cb])
    findOne(cname, qobj, [cb])
    findOne(cname, qobj, hints, [cb])
    findOne(cname, qobj, qobjarr, [cb])
    findOne(cname, qobj, qobjarr, hints, [cb])

__Arguments__

 * {String} cname Name of collection
 * {Object} qobj Main JSON query object
 * {Array} `[orarr]` Array of additional OR query objects (joined with OR predicate).
 * {Object} `[hints]` JSON object with query hints.
 * {Function} cb Callback args: (error, obj)
             `obj`  Retrieved JSON object or NULL if it is not found.

__Return__

 * If callback is provided returns {undefined}
 * If no callback is provided returns found {Object} or {null}

-----------------------------------
<a name="findOne"/>
### update(cname, qobj, orarr, hints, cb)
Convenient method to execute update queries.

 * `$set` Field set operation:
    - {some fields for selection, '$set' : {'field1' : {obj}, ...,  'field1' : {obj}}}
 * `$upsert` Atomic upsert. If matching records are found it will be '$set' operation,
 otherwise new record will be inserted with fields specified by argment object.
    - {.., '$upsert' : {'field1' : val1, 'fieldN' : valN}}
 * `$inc` Increment operation. Only number types are supported.
    - {some fields for selection, '$inc' : {'field1' : number, ...,  'field1' : {number}}
 * `$dropall` In-place record removal operation.
    - {some fields for selection, '$dropall' : true}
 * `$addToSet` | `$addToSetAll` Atomically adds value to the array only if its not in the array already.
 If containing array is missing it will be created.
    - {.., '$addToSet' : {'fpath' : val1, 'fpathN' : valN, ...}}
 * `$pull` | `pullAll` Atomically removes all occurrences of value from field, if field is an array.
    - {.., '$pull' : {'fpath' : val1, 'fpathN' : valN, ...}}

Call variations of update():

    update(cname, [cb])
    update(cname, qobj, [cb])
    update(cname, qobj, hints, [cb])
    update(cname, qobj, qobjarr, [cb])
    update(cname, qobj, qobjarr, hints, [cb])

__Arguments__

 * {String} cname Name of collection
 * {Object} qobj Update JSON query object
 * {Array} `[orarr]` Array of additional OR query objects (joined with OR predicate).
 * {Object} `[hints]` JSON object with query hints.
 * {Function} cb Callback args: (error, count)
             `count`  The number of updated records.

__Return__

 * If callback is provided returns {undefined}.
 * If no callback is provided returns {Number} of updated objects.


-----------------------------------

<a name="count"/>
### count(cname, qobj, orarr, hints, cb)
Convenient count(*) operation.

Call variations of count():

    count(cname, [cb])
    count(cname, qobj, [cb])
    count(cname, qobj, hints, [cb])
    count(cname, qobj, qobjarr, [cb])
    count(cname, qobj, qobjarr, hints, [cb])

__Arguments__

 * {String} cname Name of collection
 * {Object} qobj Main JSON query object
 * {Array} `[orarr]` Array of additional OR query objects (joined with OR predicate).
 * {Object} `[hints]` JSON object with query hints.
 * {Function} cb Callback args: (error, count)
              `count`:  Number of matching records.

__Return__

 * If callback is provided returns {undefined}.
 * If no callback is provided returns {Number} of matched object.

-----------------------------------

<a name="sync"/>
### sync(cb)
Synchronize entire EJDB database with disk.

__Arguments__

 * {Function} cb Callback args: (error)

-----------------------------------

<a name="dropIndexes"/>
### dropIndexes(cname, path, cb)
Drop indexes of all types for JSON field path.

__Arguments__

 * {String} cname Name of collection
 * {String} path  JSON field path
 * {Function} `[cb]` Optional callback function. Callback args: (error)

------------------------------------

<a name="optimizeIndexes"/>
### optimizeIndexes(cname, path, cb)
Optimize indexes of all types for JSON field path.
Performs B+ tree index file optimization.

 __Arguments__

  * {String} cname Name of collection
  * {String} path  JSON field path
  * {Function} `[cb]` Optional callback function. Callback args: (error)

-----------------------------------

<a name="ensureIndex"/>
### ensureStringIndex(cname, path, cb)
### ensureIStringIndex(cname, path, cb)
### ensureNumberIndex(cname, path, cb)
### ensureArrayIndex(cname, path, cb)

Ensure index presence of String|Number|Array type for JSON field path.
`IString` is the special type of String index for case insensitive matching.

 __Arguments__

  * {String} cname Name of collection
  * {String} path  JSON field path
  * {Function} `[cb]` Optional callback function. Callback args: (error)

-----------------------------------

<a name="rebuildIndex"/>
### rebuildStringIndex(cname, path, cb)
### rebuildIStringIndex(cname, path, cb)
### rebuildNumberIndex(cname, path, cb)
### rebuildArrayIndex(cname, path, cb)

Rebuild index of String|Number|Array type for JSON field path.
`IString` is the special type of String index for case insensitive matching.

 __Arguments__

  * {String} cname Name of collection
  * {String} path  JSON field path
  * {Function} `[cb]` Optional callback function. Callback args: (error)

-----------------------------------

<a name="dropIndex"/>
### dropStringIndex(cname, path, cb)
### dropIStringIndex(cname, path, cb)
### dropNumberIndex(cname, path, cb)
### dropArrayIndex(cname, path, cb)

Drop index of String|Number|Array type for JSON field path.
`IString` is the special type of String index for case insensitive matching.

 __Arguments__

  * {String} cname Name of collection
  * {String} path  JSON field path
  * {Function} `[cb]` Optional callback function. Callback args: (error)

-----------------------------------


EJDB Python binding
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
**[EJDB Python 2.7/3.x binding page](https://github.com/Softmotions/ejdb/blob/master/pyejdb/README.md)**

EJDB Lua binding
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
* **[Lua binding](https://github.com/Softmotions/ejdb/blob/master/luaejdb/README.md)**


EJDB C Library
==================================

One snippet intro
-----------------------------------

~~~~~~
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
~~~~~~

You can save this code in `csnippet.c` And build:


```sh
gcc -std=c99 -Wall -pedantic  -c -o csnippet.o csnippet.c
gcc -o csnippet csnippet.o -ltcejdb
```

Building & Installation
--------------------------------

[Installing on Debian/Ubuntu](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation)

Manual installation
-------------------------------

### Prerequisites
**System libraries:**

* gcc
* zlib-dev

### Build and install

~~~~~~
   cd ./tcejdb
   ./configure --prefix=<installation prefix> && make && make check
   make install
~~~~~~
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

Related software
------------------------------------
[Connect session store backed by EJDB database](https://github.com/Softmotions/connect-session-ejdb)


