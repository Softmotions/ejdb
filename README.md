# EJDB 2.0

[![Join Telegram](https://img.shields.io/badge/join-ejdb2%20telegram-0088cc.svg)](https://tlg.name/ejdb2)
[![license](https://img.shields.io/github/license/Softmotions/ejdb.svg)](https://github.com/Softmotions/ejdb/blob/master/LICENSE)
![maintained](https://img.shields.io/maintenance/yes/2022.svg)

EJDB2 is an embeddable JSON database engine published under MIT license.

[The Story of the IT-depression, birds and EJDB 2.0](https://medium.com/@adamansky/ejdb2-41670e80897c)

* C11 API
* Single file database
* Online backups support
* 500K library size for Android
* [iOS](https://github.com/Softmotions/EJDB2Swift) / [Android](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_android/test) / [React Native](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_react_native) / [Flutter](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_flutter) integration
* Simple but powerful query language (JQL) as well as support of the following standards:
  * [rfc6902](https://tools.ietf.org/html/rfc6902) JSON Patch
  * [rfc7386](https://tools.ietf.org/html/rfc7386) JSON Merge patch
  * [rfc6901](https://tools.ietf.org/html/rfc6901) JSON Path
* [Support of collection joins](#jql-collection-joins)
* Powered by [iowow.io](http://iowow.io) - The persistent key/value storage engine
* HTTP REST/Websockets endpoints powered by [IWNET](https://github.com/Softmotions/iwnet) and [BearSSL](https://github.com/Softmotions/BearSSL).
* JSON documents are stored in using fast and compact [binn](https://github.com/liteserver/binn) binary format

---
* [Native language bindings](#native-language-bindings)
* Supported platforms
  * [macOS](#osx)
  * [iOS](https://github.com/Softmotions/EJDB2Swift)
  * [Linux](#linux)
  * [Android](#android)
  * [Windows](#windows)
* **[JQL query language](#jql)**
  * [Grammar](#jql-grammar)
  * [Quick into](#jql-quick-introduction)
  * [Data modification](#jql-data-modification)
  * [Projections](#jql-projections)
  * [Collection joins](#jql-collection-joins)
  * [Sorting](#jql-sorting)
  * [Query options](#jql-options)
* [Indexes and performance](#jql-indexes-and-performance-tips)
* [Network API](#http-restwebsocket-api-endpoint)
  * [HTTP API](#http-api)
  * [Websockets API](#websocket-api)
* [C API](#c-api)
* [License](#license)
---

[![EJDB2 Presentation](https://iowow.softmotions.com/articles/ejdb-presentation-cover.png)](https://iowow.softmotions.com/articles/ejdb/)

## EJDB2 platforms matrix

|              | Linux              | macOS               | iOS                | Android            | Windows            |
| ---          | ---                | ---                 | ---                | ---                | ---                |
| C library    | :heavy_check_mark: | :heavy_check_mark:  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark:<sup>1</sup> |
| NodeJS       | :heavy_check_mark: | :heavy_check_mark:  |                    |                    | :x:<sup>3</sup>    |
| DartVM       | :heavy_check_mark: | :heavy_check_mark:<sup>2</sup> |         |                    | :x:<sup>3</sup>    |
| Flutter      |                    |                     | :heavy_check_mark: | :heavy_check_mark: |                    |
| React Native |                    |                     | :x:<sup>4</sup>    | :heavy_check_mark: |                    |
| Swift        | :heavy_check_mark: | :heavy_check_mark:  | :heavy_check_mark: |                    |                    |
| Java         | :heavy_check_mark: | :heavy_check_mark:  |                    | :heavy_check_mark: | :heavy_check_mark:<sup>2</sup> |


<br> `[1]` No HTTP/Websocket support [#257](https://github.com/Softmotions/ejdb/issues/257)
<br> `[2]` Binaries are not distributed with dart `pub.` You can build it [manually](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_node#how-build-it-manually)
<br> `[3]` Can be build, but needed a linkage with windows node/dart `libs`.
<br> `[4]` Porting in progress [#273](https://github.com/Softmotions/ejdb/issues/273)

## Native language bindings

* [NodeJS](https://www.npmjs.com/package/ejdb2_node)
* [Dart](https://pub.dartlang.org/packages/ejdb2_dart)
* [Java](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_jni/README.md)
* [Android support](#android)
* [Swift | iOS](https://github.com/Softmotions/EJDB2Swift)
* [React Native](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_react_native)
* [Flutter](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_flutter)

### Unofficial EJDB2 language bindings

* Go
  * https://github.com/memmaker/go-ejdb2
* Rust 
  * https://crates.io/crates/ejdb2
* .Net
  * https://github.com/kmvi/ejdb2-csharp
* Haskell
  * https://github.com/cescobaz/ejdb2haskell
  * https://hackage.haskell.org/package/ejdb2-binding
* [Pharo](https://pharo.org)
  * https://github.com/pharo-nosql/pharo-ejdb
* Lua
  * https://github.com/chriku/ejdb-lua

## Status

* **EJDB 2.0 core engine is well tested and used in various heavily loaded deployments**
* Tested on `Linux`, `macOS` and `FreeBSD`. [Has limited Windows support](./WINDOWS.md)
* Old EJDB 1.x version can be found in separate [ejdb_1.x](https://github.com/Softmotions/ejdb/tree/ejdb_1.x) branch.
  We are not maintaining ejdb 1.x.

## Used by

* [Wirow video conferencing platform](https://github.com/wirow-io/wirow-server/)

Are you using EJDB? [Let me know!](mailto:info@softmotions.com)

## macOS

EJDB2 code ported and tested on `High Sierra` / `Mojave` / `Catalina`

[EJDB2 Swift binding](https://github.com/Softmotions/EJDB2Swift) for MacOS, iOS and Linux. 
Swift binding is outdated at now. Looking for contributors.

```
brew install ejdb
```

## Building from sources 

cmake v3.12 or higher required

```
git clone --recurse-submodules git@github.com:Softmotions/ejdb.git

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make install
```

## Linux

#### Building debian packages

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_DEB=ON
make package
```

#### RPM based Linux distributions
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_RPM=ON
make package
```

## Windows
EJDB2 can be cross-compiled for windows

**Note:** HTTP/Websocket network API is disabled and not yet supported

Nodejs/Dart bindings not yet ported to Windows.

**[Cross-compilation Guide for Windows](./WINDOWS.md)**


## IWSTART

IWSTART is an automatic CMake initial project generator for C projects based on [iowow](https://github.com/Softmotions/iowow) / [iwnet](https://github.com/Softmotions/iwnet) / [ejdb2](https://github.com/Softmotions/ejdb) libs.

https://github.com/Softmotions/iwstart



# Android

* [Flutter binding](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_flutter)
* [React Native binding](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_react_native)

## Sample Android application

* https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_android/test

* https://github.com/Softmotions/ejdb_android_todo_app


# JQL

EJDB query language (JQL) syntax inspired by ideas behind XPath and Unix shell pipes.
It designed for easy querying and updating sets of JSON documents.

## JQL grammar

JQL parser created created by
[peg/leg — recursive-descent parser generators for C](http://piumarta.com/software/peg/) Here is the formal parser grammar: https://github.com/Softmotions/ejdb/blob/master/src/jql/jqp.leg

## Non formal JQL grammar adapted for brief overview

Notation used below is based on SQL syntax description:

Rule | Description
--- | ---
`' '` | String in single quotes denotes unquoted string literal as part of query.
<code>{ a &#124; b }</code> | Curly brackets enclose two or more required alternative choices, separated by vertical bars.
<code>[ ]</code> | Square brackets indicate an optional element or clause. Multiple elements or clauses are separated by vertical bars.
<code>&#124;</code> | Vertical bars separate two or more alternative syntax elements.
<code>...</code> |  Ellipses indicate that the preceding element can be repeated. The repetition is unlimited unless otherwise indicated.
<code>( )</code> | Parentheses are grouping symbols.
Unquoted word in lower case| Denotes semantic of some query part. For example: `placeholder_name` - name of any placeholder.
```
QUERY = FILTERS [ '|' APPLY ] [ '|' PROJECTIONS ] [ '|' OPTS ];

STR = { quoted_string | unquoted_string };

JSONVAL = json_value;

PLACEHOLDER = { ':'placeholder_name | '?' }

FILTERS = FILTER [{ and | or } [ not ] FILTER];

  FILTER = [@collection_name]/NODE[/NODE]...;

  NODE = { '*' | '**' | NODE_EXPRESSION | STR };

  NODE_EXPRESSION = '[' NODE_EXPR_LEFT OP NODE_EXPR_RIGHT ']'
                        [{ and | or } [ not ] NODE_EXPRESSION]...;

  OP =   [ '!' ] { '=' | '>=' | '<=' | '>' | '<' | ~ }
      | [ '!' ] { 'eq' | 'gte' | 'lte' | 'gt' | 'lt' }
      | [ not ] { 'in' | 'ni' | 're' };

  NODE_EXPR_LEFT = { '*' | '**' | STR | NODE_KEY_EXPR };

  NODE_KEY_EXPR = '[' '*' OP NODE_EXPR_RIGHT ']'

  NODE_EXPR_RIGHT =  JSONVAL | STR | PLACEHOLDER

APPLY = { 'apply' | 'upsert' } { PLACEHOLDER | json_object | json_array  } | 'del'

OPTS = { 'skip' n | 'limit' n | 'count' | 'noidx' | 'inverse' | ORDERBY }...

  ORDERBY = { 'asc' | 'desc' } PLACEHOLDER | json_path

PROJECTIONS = PROJECTION [ {'+' | '-'} PROJECTION ]

  PROJECTION = 'all' | json_path

```

* `json_value`: Any valid JSON value: object, array, string, bool, number.
* `json_path`: Simplified JSON pointer. Eg.: `/foo/bar` or `/foo/"bar with spaces"/`
* `*` in context of `NODE`: Any JSON object key name at particular nesting level.
* `**` in context of `NODE`: Any JSON object key name at arbitrary nesting level.
* `*` in context of `NODE_EXPR_LEFT`: Key name at specific level.
* `**` in context of `NODE_EXPR_LEFT`: Nested array value of array element under specific key.

## JQL quick introduction

Lets play with some very basic data and queries.
For simplicity we will use ejdb websocket network API which provides us a kind of interactive CLI. The same job can be done using pure `C` API too (`ejdb2.h jql.h`).

NOTE: Take a look into [JQL test cases](https://github.com/Softmotions/ejdb/blob/master/src/jql/tests/jql_test1.c) for more examples.

```json
{
  "firstName": "John",
  "lastName": "Doe",
  "age": 28,
  "pets": [
    {"name": "Rexy rex", "kind": "dog", "likes": ["bones", "jumping", "toys"]},
    {"name": "Grenny", "kind": "parrot", "likes": ["green color", "night", "toys"]}
  ]
}
```
Save json as `sample.json` then upload it the `family` collection:

```sh
# Start HTTP/WS server protected by some access token
./jbs -a 'myaccess01'
8 Mar 16:15:58.601 INFO: HTTP/WS endpoint at localhost:9191
```

Server can be accessed using HTTP or Websocket endpoint. [More info](https://github.com/Softmotions/ejdb/blob/master/src/jbr/README.md)

```sh
curl -d '@sample.json' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191/family
```

We can play around using interactive [wscat](https://www.npmjs.com/package/@softmotions/wscat) websocket client.

```sh
wscat  -H 'X-Access-Token:myaccess01' -c http://localhost:9191
connected (press CTRL+C to quit)
> k info
< k     {
 "version": "2.0.0",
 "file": "db.jb",
 "size": 8192,
 "collections": [
  {
   "name": "family",
   "dbid": 3,
   "rnum": 1,
   "indexes": []
  }
 ]
}

> k get family 1
< k     1       {
 "firstName": "John",
 "lastName": "Doe",
 "age": 28,
 "pets": [
  {
   "name": "Rexy rex",
   "kind": "dog",
   "likes": [
    "bones",
    "jumping",
    "toys"
   ]
  },
  {
   "name": "Grenny",
   "kind": "parrot",
   "likes": [
    "green color",
    "night",
    "toys"
   ]
  }
 ]
}
```

Note about the `k` prefix before every command; It is an arbitrary key chosen by client and designated to identify particular
websocket request, this key will be returned with response to request and allows client to
identify that response for his particular request. [More info](https://github.com/Softmotions/ejdb/blob/master/src/jbr/README.md)

Query command over websocket has the following format:

```
<key> query <collection> <query>
```

So we will consider only `<query>` part in this document.

### Get all elements in collection
```
k query family /*
```
or
```
k query family /**
```
or specify collection name in query explicitly
```
k @family/*
```

We can execute query by HTTP `POST` request
```
curl --data-raw '@family/[firstName = John]' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191

1	{"firstName":"John","lastName":"Doe","age":28,"pets":[{"name":"Rexy rex","kind":"dog","likes":["bones","jumping","toys"]},{"name":"Grenny","kind":"parrot","likes":["green color","night","toys"]}]}
```

### Set the maximum number of elements in result set

```
k @family/* | limit 10
```

### Get documents where specified json path exists

Element at index `1` exists in `likes` array within a `pets` sub-object
```
> k query family /pets/*/likes/1
< k     1       {"firstName":"John"...
```

Element at index `1` exists in `likes` array at any `likes` nesting level
```
> k query family /**/likes/1
< k     1       {"firstName":"John"...
```

**From this point and below I will omit websocket specific prefix `k query family` and
consider only JQL queries.**


### Get documents by primary key

In order to get documents by primary key the following options are available:

1. Use API call `ejdb_get()`
    ```ts
     const doc = await db.get('users', 112);
    ```

1. Use the special query construction: `/=:?` or `@collection/=:?`

Get document from `users` collection with primary key `112`
```
> k @users/=112
```

Update tags array for document in `jobs` collection (TypeScript):
```ts
 await db.createQuery('@jobs/ = :? | apply :? | count')
    .setNumber(0, id)
    .setJSON(1, { tags })
    .completionPromise();
```

Array of primary keys can also be used for matching:

```ts
 await db.createQuery('@jobs/ = :?| apply :? | count')
    .setJSON(0, [23, 1, 2])
    .setJSON(1, { tags })
    .completionPromise();
```

### Matching JSON entry values

Below is a set of self explaining queries:

```
/pets/*/[name = "Rexy rex"]

/pets/*/[name eq "Rexy rex"]

/pets/*/[name = "Rexy rex" or name = Grenny]
```
Note about quotes around words with spaces.

Get all documents where owner `age` greater than `20` and have some pet who like `bones` or `toys`
```
/[age > 20] and /pets/*/likes/[** in ["bones", "toys"]]
```
Here `**` denotes some element in `likes` array.

`ni` is the inverse operator to `in`.
Get documents where `bones` somewhere in `likes` array.
```
/pets/*/[likes ni "bones"]
```

We can create more complicated filters
```
( /[age <= 20] or /[lastName re "Do.*"] )
  and /pets/*/likes/[** in ["bones", "toys"]]
```
Note about grouping parentheses and regular expression matching using `re` operator.

`~` is a prefix matching operator (Since ejdb `v2.0.53`).
Prefix matching can benefit from using indexes.

Get documents where `/lastName` starts with `"Do"`.
```
/[lastName ~ Do]
```

### Arrays and maps can be matched as is

Filter documents with `likes` array exactly matched to `["bones","jumping","toys"]`
```
/**/[likes = ["bones","jumping","toys"]]
```
Matching algorithms for arrays and maps are different:

* Array elements are matched from start to end. In equal arrays
  all values at the same index should be equal.
* Object maps matching consists of the following steps:
  * Lexicographically sort object keys in both maps.
  * Do matching keys and its values starting from the lowest key.
  * If all corresponding keys and values in one map are fully matched to ones in other
    and vice versa, maps considered to be equal.
    For example: `{"f":"d","e":"j"}` and `{"e":"j","f":"d"}` are equal maps.

### Conditions on key names

Find JSON document having `firstName` key at root level.
```
/[* = "firstName"]
```
I this context `*` denotes a key name.

You can use conditions on key name and key value at the same time:
```
/[[* = "firstName"] = John]
```

Key name can be either `firstName` or `lastName` but should have `John` value in any case.
```
/[[* in ["firstName", "lastName"]] = John]
```

It may be useful in queries with dynamic placeholders (C API):
```
/[[* = :keyName] = :keyValue]
```

## JQL data modification

`APPLY` section responsible for modification of documents content.

```
APPLY = ({'apply' | `upsert`} { PLACEHOLDER | json_object | json_array  }) | 'del'
```

JSON patch specs conformed to `rfc7386` or `rfc6902` specifications followed after `apply` keyword.

Let's add `address` object to all matched document
```
/[firstName = John] | apply {"address":{"city":"New York", "street":""}}
```

If JSON object is an argument of `apply` section it will be treated as merge match (`rfc7386`) otherwise
it should be array which denotes `rfc6902` JSON patch. Placeholders also supported by `apply` section.
```
/* | apply :?
```

Set the street name in `address`
```
/[firstName = John] | apply [{"op":"replace", "path":"/address/street", "value":"Fifth Avenue"}]
```

Add `Neo` fish to the set of John's `pets`
```
/[firstName = John]
| apply [{"op":"add", "path":"/pets/-", "value": {"name":"Neo", "kind":"fish"}}]
```

`upsert` updates existing document by given json argument used as merge patch
         or inserts provided json argument as new document instance.

```
/[firstName = John] | upsert {"firstName": "John", "address":{"city":"New York"}}
```

### Non standard JSON patch extensions

#### increment

Increments numeric value identified by JSON path by specified value.

Example:
```
 Document:  {"foo": 1}
 Patch:     [{"op": "increment", "path": "/foo", "value": 2}]
 Result:    {"foo": 3}
```
#### add_create

Same as JSON patch `add` but creates intermediate object nodes for missing JSON path segments.

Example:
```
Document: {"foo": {"bar": 1}}
Patch:    [{"op": "add_create", "path": "/foo/zaz/gaz", "value": 22}]
Result:   {"foo":{"bar":1,"zaz":{"gaz":22}}}
```

Example:
```
Document: {"foo": {"bar": 1}}
Patch:    [{"op": "add_create", "path": "/foo/bar/gaz", "value": 22}]
Result:   Error since element pointed by /foo/bar is not an object
```

#### swap

Swaps two values of JSON document starting from `from` path.

Swapping rules

1. If value pointed by `from` not exists error will be raised.
1. If value pointed by `path` not exists it will be set by value from `from` path,
  then object pointed by `from` path will be removed.
1. If both values pointed by `from` and `path` are presented they will be swapped.

Example:

```
Document: {"foo": ["bar"], "baz": {"gaz": 11}}
Patch:    [{"op": "swap", "from": "/foo/0", "path": "/baz/gaz"}]
Result:   {"foo": [11], "baz": {"gaz": "bar"}}
```

Example (Demo of rule 2):

```
Document: {"foo": ["bar"], "baz": {"gaz": 11}}
Patch:    [{"op": "swap", "from": "/foo/0", "path": "/baz/zaz"}]
Result:   {"foo":[],"baz":{"gaz":11,"zaz":"bar"}}
```

### Removing documents

Use `del` keyword to remove matched elements from collection:
```
/FILTERS | del
```

Example:
```
> k add family {"firstName":"Jack"}
< k     2
> k query family /[firstName re "Ja.*"]
< k     2       {"firstName":"Jack"}

# Remove selected elements from collection
> k query family /[firstName=Jack] | del
< k     2       {"firstName":"Jack"}
```

## JQL projections

```
PROJECTIONS = PROJECTION [ {'+' | '-'} PROJECTION ]

  PROJECTION = 'all' | json_path | join_clause
```

Projection allows to get only subset of JSON document excluding not needed data.

**Query placeholders API is supported in projections.**

Lets add one more document to our collection:

```sh
$ cat << EOF | curl -d @- -H'X-Access-Token:myaccess01' -X POST http://localhost:9191/family
{
"firstName":"Jack",
"lastName":"Parker",
"age":35,
"pets":[{"name":"Sonic", "kind":"mouse", "likes":[]}]
}
EOF
```
Now query only pet owners firstName and lastName from collection.

```
> k query family /* | /{firstName,lastName}

< k     3       {"firstName":"Jack","lastName":"Parker"}
< k     1       {"firstName":"John","lastName":"Doe"}
< k
```

Add `pets` array for every document
```
> k query family /* | /{firstName,lastName} + /pets

< k     3       {"firstName":"Jack","lastName":"Parker","pets":[...
< k     1       {"firstName":"John","lastName":"Doe","pets":[...
```

Exclude only `pets` field from documents
```
> k query family /* | all - /pets

< k     3       {"firstName":"Jack","lastName":"Parker","age":35}
< k     1       {"firstName":"John","lastName":"Doe","age":28,"address":{"city":"New York","street":"Fifth Avenue"}}
< k
```
Here `all` keyword used denoting whole document.

Get `age` and the first pet in `pets` array.
```
> k query family /[age > 20] | /age + /pets/0

< k     3       {"age":35,"pets":[{"name":"Sonic","kind":"mouse","likes":[]}]}
< k     1       {"age":28,"pets":[{"name":"Rexy rex","kind":"dog","likes":["bones","jumping","toys"]}]}
< k
```

## JQL collection joins

Join materializes reference to document to a real document objects which will replace reference inplace.

Documents are joined by their primary keys only.

Reference keys should be stored in referrer document as number or string field.

Joins can be specified as part of projection expression
in the following form:

```
/.../field<collection
```
Where

* `field` &dash; JSON field contains primary key of joined document.
* `<` &dash; The special mark symbol which instructs EJDB engine to replace `field` key by body of joined document.
* `collection` &dash; name of DB collection where joined documents located.

A referrer document will be untouched if associated document is not found.

Here is the simple demonstration of collection joins in our interactive websocket shell:

```
> k add artists {"name":"Leonardo Da Vinci", "years":[1452,1519]}
< k     1
> k add paintings {"name":"Mona Lisa", "year":1490, "origin":"Italy", "artist": 1}
< k     1
> k add paintings {"name":"Madonna Litta - Madonna And The Child", "year":1490, "origin":"Italy", "artist": 1}
< k     2

# Lists paintings documents

> k @paintings/*
< k     2       {"name":"Madonna Litta - Madonna And The Child","year":1490,"origin":"Italy","artist":1}
< k     1       {"name":"Mona Lisa","year":1490,"origin":"Italy","artist":1}
< k
>

# Do simple join with artists collection

> k @paintings/* | /artist<artists
< k     2       {"name":"Madonna Litta - Madonna And The Child","year":1490,"origin":"Italy",
                  "artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}

< k     1       {"name":"Mona Lisa","year":1490,"origin":"Italy",
                  "artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k


# Strip all document fields except `name` and `artist` join

> k @paintings/* | /artist<artists + /name + /artist/*
< k     2       {"name":"Madonna Litta - Madonna And The Child","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k     1       {"name":"Mona Lisa","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k
>

# Same results as above:

> k @paintings/* | /{name, artist<artists} + /artist/*
< k     2       {"name":"Madonna Litta - Madonna And The Child","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k     1       {"name":"Mona Lisa","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k

```

Invalid references:

```
>  k add paintings {"name":"Mona Lisa2", "year":1490, "origin":"Italy", "artist": 9999}
< k     3
> k @paintings/* |  /artist<artists
< k     3       {"name":"Mona Lisa2","year":1490,"origin":"Italy","artist":9999}
< k     2       {"name":"Madonna Litta - Madonna And The Child","year":1490,"origin":"Italy","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}
< k     1       {"name":"Mona Lisa","year":1490,"origin":"Italy","artist":{"name":"Leonardo Da Vinci","years":[1452,1519]}}

```

## JQL results ordering

```
  ORDERBY = ({ 'asc' | 'desc' } PLACEHOLDER | json_path)...
```

Lets add one more document then sort documents in collection according to `firstName` ascending and `age` descending order.

```
> k add family {"firstName":"John", "lastName":"Ryan", "age":39}
< k     4
```

```
> k query family /* | /{firstName,lastName,age} | asc /firstName desc /age
< k     3       {"firstName":"Jack","lastName":"Parker","age":35}
< k     4       {"firstName":"John","lastName":"Ryan","age":39}
< k     1       {"firstName":"John","lastName":"Doe","age":28}
< k
```

`asc, desc` instructions may use indexes defined for collection to avoid a separate documents sorting stage.

## JQL Options

```
OPTS = { 'skip' n | 'limit' n | 'count' | 'noidx' | 'inverse' | ORDERBY }...
```

* `skip n` Skip first `n` records before first element in result set
* `limit n` Set max number of documents in result set
* `count` Returns only `count` of matched documents
  ```
  > k query family /* | count
  < k     3
  < k
  ```
* `noidx` Do not use any indexes for query execution.
* `inverse` By default query scans documents from most recently added to older ones.
   This option inverts scan direction to opposite and activates `noidx` mode.
   Has no effect if query has `asc/desc` sorting clauses.

## JQL Indexes and performance tips

Database index can be build for any JSON field path containing values of number or string type.
Index can be an `unique` &dash; not allowing value duplication and `non unique`.
The following index mode bit mask flags are used (defined in `ejdb2.h`):

Index mode | Description
--- | ---
<code>0x01 EJDB_IDX_UNIQUE</code> | Index is unique
<code>0x04 EJDB_IDX_STR</code> | Index for JSON `string` field value type
<code>0x08 EJDB_IDX_I64</code> | Index for `8 bytes width` signed integer field values
<code>0x10 EJDB_IDX_F64</code> | Index for `8 bytes width` signed floating point field values.

For example unique index of string type will be specified by `EJDB_IDX_UNIQUE | EJDB_IDX_STR` = `0x05`.
Index can be defined for only one value type located under specific path in json document.

Lets define non unique string index for `/lastName` path:
```
> k idx family 4 /lastName
< k
```
Index selection for queries based on set of heuristic rules.

You can always check index usage by issuing `explain` command in WS API:
```
> k explain family /[lastName=Doe] and /[age!=27]
< k     explain [INDEX] MATCHED  STR|3 /lastName EXPR1: 'lastName = Doe' INIT: IWKV_CURSOR_EQ
[INDEX] SELECTED STR|3 /lastName EXPR1: 'lastName = Doe' INIT: IWKV_CURSOR_EQ
 [COLLECTOR] PLAIN
```

The following statements are taken into account when using EJDB2 indexes:
* Only one index can be used for particular query execution
* If query consist of `or` joined part at top level or contains `negated` expressions at the top level
  of query expression - indexes will not be in use at all.
  So no indexes below:
  ```
  /[lastName != Andy]

  /[lastName = "John"] or /[lastName = Peter]

  ```
  But will be used `/lastName` index defined above
  ```
  /[lastName = Doe]

  /[lastName = Doe] and /[age = 28]

  /[lastName = Doe] and not /[age = 28]

  /[lastName = Doe] and /[age != 28]
  ```
* The following operators are supported by indexes (ejdb 2.0.x):
  * `eq, =`
  * `gt, >`
  * `gte, >=`
  * `lt, <`
  * `lte, <=`
  * `in`
  * `~` (Prefix matching since ejdb 2.0.53)

* `ORDERBY` clauses may use indexes to avoid result set sorting.
* Array fields can also be indexed. Let's outline typical use case: indexing of some entity tags:
  ```
  > k add books {"name":"Mastering Ultra", "tags":["ultra", "language", "bestseller"]}
  < k     1
  > k add books {"name":"Learn something in 24 hours", "tags":["bestseller"]}
  < k     2
  > k query books /*
  < k     2       {"name":"Learn something in 24 hours","tags":["bestseller"]}
  < k     1       {"name":"Mastering Ultra","tags":["ultra","language","bestseller"]}
  < k
  ```
  Create string index for `/tags`
  ```
  > k idx books 4 /tags
  < k
  ```
  Filter books by `bestseller` tag and show index usage in query:
  ```
  > k explain books /tags/[** in ["bestseller"]]
  < k     explain [INDEX] MATCHED  STR|4 /tags EXPR1: '** in ["bestseller"]' INIT: IWKV_CURSOR_EQ
  [INDEX] SELECTED STR|4 /tags EXPR1: '** in ["bestseller"]' INIT: IWKV_CURSOR_EQ
  [COLLECTOR] PLAIN

  < k     1       {"name":"Mastering Ultra","tags":["ultra","language","bestseller"]}
  < k     2       {"name":"Learn something in 24 hours","tags":["bestseller"]}
  < k
  ```

### Performance tip: Physical ordering of documents

All documents in collection are sorted by their primary key in `descending` order.
So if you use auto generated keys (`ejdb_put_new`) you may be sure what documents fetched as result of
full scan query will be ordered according to the time of insertion in descendant order,
unless you don't use query sorting, indexes or `inverse` keyword.

### Performance tip: Brute force scan vs indexed access

In many cases, using index may drop down the overall query performance.
Because index collection contains only document references (`id`) and engine may perform
an addition document fetching by its primary key to finish query matching.
So for not so large collections a brute scan may perform better than scan using indexes.
However, exact matching operations: `eq`, `in` and `sorting` by natural index order
will benefit from index in most cases.


### Performance tip: Get rid of unnecessary document data

If you'd like update some set of documents with `apply` or `del` operations
but don't want fetching all of them as result of query - just add `count`
modifier to the query to get rid of unnecessary data transferring and json data conversion.



# HTTP REST/Websocket API endpoint

EJDB engine provides the ability to start a separate HTTP/Websocket endpoint worker exposing network API for quering and data modifications.
SSL (TLS 1.2) is supported by `jbs` server.

The easiest way to expose database over the network is use the standalone `jbs` server. (Of course if you want to avoid `C API` integration).

## jbs server

```
Usage:

	 ./jbs [options]

	-v, --version		Print program version.
	-f, --file=<>		Database file path. Default: ejdb2.db
	-p, --port=NUM		HTTP server port numer. Default: 9191
	-l, --listen=<>		Network address server will listen. Default: localhost
	-k, --key=<>		PEM private key file for TLS 1.2 HTTP server.
	-c, --certs=<>		PEM certificates file for TLS 1.2 HTTP server.
	-a, --access=TOKEN|@FILE		Access token to match 'X-Access-Token' HTTP header value.
	-r, --access-read		Allows unrestricted read-only data access.
	-C, --cors		Enable COSR response headers for HTTP server
	-t, --trunc		Cleanup/reset database file on open.
	-w, --wal		use the write ahead log (WAL). Used to provide data durability.

Advanced options:
	-S, --sbz=NUM		Max sorting buffer size. If exceeded, an overflow temp file for data will be created.
                  Default: 16777216, min: 1048576
	-D, --dsz=NUM		Initial size of buffer to process/store document on queries. Preferable average size of document. 
                  Default: 65536, min: 16384
	-T, --trylock Exit with error if database is locked by another process. 
                If not set, current process will wait for lock release.

```

## HTTP API

HTTP endpoint may be protected by a token specified with `--access` flag or C API `EJDB_HTTP` struct. 
If access token was set, client should provide `X-Access-Token` HTTP header.
If token is required but not provided by client `401` HTTP code will be reported. 
If access token is not matched to the token provided by client server will respond with `403` HTTP code.

## REST API

### POST /{collection}
Add a new document to the `collection`.
* `200` success. Body: a new document identifier as `int64` number

### PUT /{collection}/{id}
Replaces/store document under specific numeric `id`
* `200` on success. Empty body

### DELETE /{collection}/{id}
Removes document identified by `id` from a `collection`
* `200` on success. Empty body
* `404` document not found

### PATCH /{collection}/{id}
Patch a document identified by `id` by [rfc7396](https://tools.ietf.org/html/rfc7396),
[rfc6902](https://tools.ietf.org/html/rfc6902) data.
* `200` on success. Empty body

### GET | HEAD /{collections}/{id}
Retrieve document identified by `id` from a `collection`.
* `200` on success. Body: JSON document text.
  * `content-type:application/json`
  * `content-length:`
* `404` document not found

### POST /
Query a collection by provided query as POST body.
Body of query should contains collection name in use in the first filter element: `@collection_name/...`
Request headers:
* `X-Hints` comma separated extra hints to ejdb2 database engine.
  * `explain` Show query execution plan before first element in result set separated by `--------------------` line.
Response:
* Response data transfered using [HTTP chunked transfer encoding](https://en.wikipedia.org/wiki/Chunked_transfer_encoding)
* `200` on success.
* JSON documents separated by `\n` in the following format:
  ```
  \r\n<document id>\t<document JSON body>
  ...
  ```

Example:

```
curl -v --data-raw '@family/[age > 18]' -H 'X-Access-Token:myaccess01' http://localhost:9191
* Rebuilt URL to: http://localhost:9191/
*   Trying 127.0.0.1...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 9191 (#0)
> POST / HTTP/1.1
> Host: localhost:9191
> User-Agent: curl/7.58.0
> Accept: */*
> X-Access-Token:myaccess01
> Content-Length: 18
> Content-Type: application/x-www-form-urlencoded
>
* upload completely sent off: 18 out of 18 bytes
< HTTP/1.1 200 OK
< connection:keep-alive
< content-type:application/json
< transfer-encoding:chunked
<

4	{"firstName":"John","lastName":"Ryan","age":39}
3	{"firstName":"Jack","lastName":"Parker","age":35,"pets":[{"name":"Sonic","kind":"mouse","likes":[]}]}
1	{"firstName":"John","lastName":"Doe","age":28,"pets":[{"name":"Rexy rex","kind":"dog","likes":["bones","jumping","toys"]},{"name":"Grenny","kind":"parrot","likes":["green color","night","toys"]}],"address":{"city":"New York","street":"Fifth Avenue"}}
* Connection #0 to host localhost left intact
```

```
curl --data-raw '@family/[lastName = "Ryan"]' -H 'X-Access-Token:myaccess01' -H 'X-Hints:explain' http://localhost:9191
[INDEX] MATCHED  STR|3 /lastName EXPR1: 'lastName = "Ryan"' INIT: IWKV_CURSOR_EQ
[INDEX] SELECTED STR|3 /lastName EXPR1: 'lastName = "Ryan"' INIT: IWKV_CURSOR_EQ
 [COLLECTOR] PLAIN
--------------------
4	{"firstName":"John","lastName":"Ryan","age":39}
```

### OPTIONS /
Fetch ejdb JSON metadata and available HTTP methods in `Allow` response header.
Example:
```
curl -X OPTIONS -H 'X-Access-Token:myaccess01'  http://localhost:9191/
{
 "version": "2.0.0",
 "file": "db.jb",
 "size": 16384,
 "collections": [
  {
   "name": "family",
   "dbid": 3,
   "rnum": 3,
   "indexes": [
    {
     "ptr": "/lastName",
     "mode": 4,
     "idbf": 64,
     "dbid": 4,
     "rnum": 3
    }
   ]
  }
 ]
}
```

## Websocket API

EJDB supports simple text based protocol over HTTP websocket protocol.
You can use interactive websocket CLI tool [wscat](https://www.npmjs.com/package/@softmotions/wscat) to communicate with server by hands.

### Commands

#### ?
Will respond with the following help text message:
```
wscat  -H 'X-Access-Token:myaccess01' -c http://localhost:9191
> ?
<
<key> info
<key> get     <collection> <id>
<key> set     <collection> <id> <document json>
<key> add     <collection> <document json>
<key> del     <collection> <id>
<key> patch   <collection> <id> <patch json>
<key> idx     <collection> <mode> <path>
<key> rmi     <collection> <mode> <path>
<key> rmc     <collection>
<key> query   <collection> <query>
<key> explain <collection> <query>
<key> <query>
>
```

Note about `<key>` prefix before every command; It is an arbitrary key chosen by client and designated to identify particular websocket request, this key will be returned with response to request and allows client to identify that response for his particular request.

Errors are returned in the following format:
```
<key> ERROR: <error description>
```

#### `<key> info`
Get database metadatas as JSON document.

#### `<key> get     <collection> <id>`
Retrieve document identified by `id` from a `collection`.
If document is not found `IWKV_ERROR_NOTFOUND` will be returned.

Example:
```
> k get family 3
< k     3       {
 "firstName": "Jack",
 "lastName": "Parker",
 "age": 35,
 "pets": [
  {
   "name": "Sonic",
   "kind": "mouse",
   "likes": []
  }
 ]
}
```
If document not found we will get error:
```
> k get family 55
< k ERROR: Key not found. (IWKV_ERROR_NOTFOUND)
>
```

#### `<key> set     <collection> <id> <document json>`
Replaces/add document under specific numeric `id`.
`Collection` will be created automatically if not exists.

#### `<key> add     <collection> <document json>`
Add new document to `<collection>` New `id` of document will be generated
and returned as response. `Collection> will be created automatically if not exists.

Example:
```
> k add mycollection {"foo":"bar"}
< k     1
> k add mycollection {"foo":"bar"}
< k     2
>
```

#### `<key> del     <collection> <id>`
Remove document identified by `id` from the `collection`.
If document is not found `IWKV_ERROR_NOTFOUND` will be returned.

#### `<key> patch   <collection> <id> <patch json>`
Apply [rfc7396](https://tools.ietf.org/html/rfc7396) or
[rfc6902](https://tools.ietf.org/html/rfc6902) patch to the document identified by `id`.
If document is not found `IWKV_ERROR_NOTFOUND` will be returned.

#### `<key> query   <collection> <query>`
Execute query on documents in specified `collection`.
**Response:** A set of WS messages with document boidies terminated by the last
message with empty body.
```
> k query family /* | /firstName
< k     4       {"firstName":"John"}
< k     3       {"firstName":"Jack"}
< k     1       {"firstName":"John"}
< k
```
Note about last message: `<key>` with no body.

#### `<key> explain <collection> <query>`
Same as `<key> query   <collection> <query>` but the first response message will
be prefixed by `<key> explain` and contains query execution plan.

Example:
```
> k explain family /* | /firstName
< k     explain [INDEX] NO [COLLECTOR] PLAIN

< k     4       {"firstName":"John"}
< k     3       {"firstName":"Jack"}
< k     1       {"firstName":"John"}
< k
```

#### <key> <query>
Execute query text. Body of query should contains collection name in use in the first filter element: `@collection_name/...`. Behavior is the same as for: `<key> query   <collection> <query>`

#### `<key> idx     <collection> <mode> <path>`
Ensure index with specified `mode` (bitmask flag) for given json `path` and `collection`.
Collection will be created if not exists.

Index mode | Description
--- | ---
<code>0x01 EJDB_IDX_UNIQUE</code> | Index is unique
<code>0x04 EJDB_IDX_STR</code> | Index for JSON `string` field value type
<code>0x08 EJDB_IDX_I64</code> | Index for `8 bytes width` signed integer field values
<code>0x10 EJDB_IDX_F64</code> | Index for `8 bytes width` signed floating point field values.

##### Example
Set unique string index `(0x01 & 0x04) = 5` on `/name` JSON field:
```
k idx mycollection 5 /name
```

#### `<key> rmi     <collection> <mode> <path>`
Remove index with specified `mode` (bitmask flag) for given json `path` and `collection`.
Return error if given index not found.

#### `<key> rmc     <collection>`
Remove collection and all of its data.
Note: If `collection` is not found no errors will be reported.




# Docker support

If you have [Docker]("https://www.docker.com/") installed, you can build a Docker image and run it in a container

```
cd docker
docker build -t ejdb2 .
docker run -d -p 9191:9191 --name myEJDB ejdb2 --access myAccessKey
```

or get an image of `ejdb2` directly from the Docker Hub

```
docker run -d -p 9191:9191 --name myEJDB softmotions/ejdb2 --access myAccessKey
```


# C API

EJDB can be embedded into any `C/C++` application.
`C API` documented in the following headers:

* [ejdb.h](https://github.com/Softmotions/ejdb/blob/master/src/ejdb2.h) Main API functions
* [jbl.h](https://github.com/Softmotions/ejdb/blob/master/src/jbl/jbl.h) JSON documents management API
* [jql.h](https://github.com/Softmotions/ejdb/blob/master/src/jql/jql.h) Query building API

Example application:
```c
#include <ejdb2/ejdb2.h>

#define CHECK(rc_)          \
  if (rc_) {                 \
    iwlog_ecode_error3(rc_); \
    return 1;                \
  }

static iwrc documents_visitor(EJDB_EXEC *ctx, const EJDB_DOC doc, int64_t *step) {
  // Print document to stderr
  return jbl_as_json(doc->raw, jbl_fstream_json_printer, stderr, JBL_PRINT_PRETTY);
}

int main() {

  EJDB_OPTS opts = {
    .kv = {
      .path = "example.db",
      .oflags = IWKV_TRUNC
    }
  };
  EJDB db;     // EJDB2 storage handle
  int64_t id;  // Document id placeholder
  JQL q = 0;   // Query instance
  JBL jbl = 0; // Json document

  iwrc rc = ejdb_init();
  CHECK(rc);

  rc = ejdb_open(&opts, &db);
  CHECK(rc);

  // First record
  rc = jbl_from_json(&jbl, "{\"name\":\"Bianca\", \"age\":4}");
  RCGO(rc, finish);
  rc = ejdb_put_new(db, "parrots", jbl, &id);
  RCGO(rc, finish);
  jbl_destroy(&jbl);

  // Second record
  rc = jbl_from_json(&jbl, "{\"name\":\"Darko\", \"age\":8}");
  RCGO(rc, finish);
  rc = ejdb_put_new(db, "parrots", jbl, &id);
  RCGO(rc, finish);
  jbl_destroy(&jbl);

  // Now execute a query
  rc =  jql_create(&q, "parrots", "/[age > :age]");
  RCGO(rc, finish);

  EJDB_EXEC ux = {
    .db = db,
    .q = q,
    .visitor = documents_visitor
  };

  // Set query placeholder value.
  // Actual query will be /[age > 3]
  rc = jql_set_i64(q, "age", 0, 3);
  RCGO(rc, finish);

  // Now execute the query
  rc = ejdb_exec(&ux);

finish:
  jql_destroy(&q);
  jbl_destroy(&jbl);
  ejdb_close(&db);
  CHECK(rc);
  return 0;
}
```

Compile and run:
```
gcc -std=gnu11 -Wall -pedantic -c -o example1.o example1.c
gcc -o example1 example1.o -lejdb2

./example1
{
 "name": "Darko",
 "age": 8
}{
 "name": "Bianca",
 "age": 4
}
```

# License
```

MIT License

Copyright (c) 2012-2022 Softmotions Ltd <info@softmotions.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

```

