# EJDB2 2.0 Beta

EJDB2 is an embeddable JSON database engine published under MIT license.

* C11 API
* Simple but powerful query language (JQL)
* Based on [iowow.io](http://iowow.io) persistent key/value storage engine
* Provides HTTP REST/Websockets network endpoints implemented with [facil.io](http://facil.io)



# JQL

EJDB query language (JQL) syntax inpired by ideas behind XPath and Unix shell pipes.
It designed for easy querying and updating sets of JSON documents.

## JQL grammar

JQL parser created created by
[peg/leg — recursive-descent parser generators for C](http://piumarta.com/software/peg/) Here is the formal parser grammar: https://github.com/Softmotions/ejdb/blob/ejdb2/src/jql/jqp.leg

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

  OP =   [ '!' ] { '=' | '>=' | '<=' | '>' | '<' }
      | [ '!' ] { 'eq' | 'gte' | 'lte' | 'gt' | 'lt' }
      | [ not ] { 'in' | 'ni' | 're' };

  NODE_EXPR_LEFT = { '*' | '**' | STR | NODE_KEY_EXPR };

  NODE_KEY_EXPR = '[' '*' OP NODE_EXPR_RIGHT ']'

  NODE_EXPR_RIGHT =  JSONVAL | STR | PLACEHOLDER

APPLY = 'apply' { PLACEHOLDER | json_object | json_array  } | 'del'

OPTS = { 'skip' n | 'limit' n | 'count' | 'noidx' | ORDERBY }...

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

NOTE: Take a look into [JQL test cases](./tests/jql_test1.c) for more examples.

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

Server can be accessed using HTTP or Websocket endpoint. [More info](../jbr/README.md)

```sh
curl -d '@sample.json' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191/family
```

We can play around using interactive [wscat](https://www.npmjs.com/package/@softmotions/wscat) websocket client.

```sh
wscat  -H 'X-Access-Token:myaccess01' -q -c http://localhost:9191
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

Note about the `k` prefix before every command; It is an arbitrary key choosen by client and designated to identify particular websocket request, this key will be returned with response to request and allows client to identify that response for his particular request. [More info](../jbr/README.md)

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
curl --data-raw '@family/[firstName=John]' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191

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

### Arrays and maps can be matched as is

Filter documents with `likes` array exactly matched to `["bones","jumping","toys"]`
```
/**/[likes = ["bones","jumping","toys"]]
```
Matching algorithms for arrays and maps are different:

* Array elements are fully matched from start to end. In equal arrays
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
APPLY = ('apply' { PLACEHOLDER | json_object | json_array  }) | 'del'
```

JSON patch specs conformed to `rfc7386` or `rfc6902` specifications followed after `apply` keyword.

Let's add `address` object to all matched document
```
/[firstName=John] | apply {"address":{"city":"New York", "street":""}}
```

If JSON object is an argument of `apply` section it will be treated as merge match (`rfc7386`) otherwise it should be array which denotes `rfc6902` JSON patch. Placegolders also supported by `apply` section.
```
/* | apply :?
```

Set the street name in `address`
```
/[firstName=John] | apply [{"op":"replace", "path":"/address/street", "value":"Fifth Avenue"}]
```

Add `Neo` fish to the set of John's `pets`
```
/[firstName=John]
| apply [{"op":"add", "path":"/pets/-", "value": {"name":"Neo", "kind":"fish"}}]
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

  PROJECTION = 'all' | json_path
```

Projection allow to get only subset of JSON document excluding not needed data.

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

## JQL sorting

```
  ORDERBY = ({ 'asc' | 'desc' } PLACEHOLDER | json_path)...
```

Lets add one more document then sort documents in collection by `firstName` ascending and `age` descending.

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
OPTS = { 'skip' n | 'limit' n | 'count' | 'noidx' | ORDERBY }...
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


## JQL Indexing and performance tips

Database index can be build for any JSON field path of number or string type.
Index can be an `unique` &dash; not allowing indexed values duplication and `non unique`.
The following index mode bit mask flags are used (defined in `ejdb2.h`):

Index mode | Description
--- | ---
<code>0x01 EJDB_IDX_UNIQUE</code> | Index is unique
<code>0x04 EJDB_IDX_STR</code> | Index for JSON `string` field value type
<code>0x08 EJDB_IDX_I64</code> | Index for `8 bytes width` signed integer field values
<code>0x10 EJDB_IDX_F64</code> | Index for `8 bytes width` signed floating point field values.

For example mode specifies unique index of string type will be `EJDB_IDX_UNIQUE | EJDB_IDX_STR` = `0x05`. Index creation operation may define index for only one type.

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
* Only one index can be used for particular query
* If query consist of `or` joined parts or contains `negated` at top level indexes will not be used.
  No indexes below:
  ```
  /[lastName != Andy]

  /[lastName = "John"] or /[lastName = Peter]
  ```
  Will use `/lastName` defined above
  ```
  /[lastName = Doe]

  /[lastName = Doe] and /[age = 28]

  /[lastName = Doe] and /[age != 28]
  ```
* The ony following operators are supported by indexes (ejdb 2.0.x):
  * `eq, =`
  * `gt, >`
  * `gte, >=`
  * `lt, <`
  * `lte, <=`
  * `in`
* `ORDERBY` clauses may use indexes to avoid result set sorting
* Array fields can also be indexed. Let's outline a typical use case: indexing of some  entity tags:
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

**NOTE:** In many cases, using index may drop down the overall query performance. Because index collection contains only document references (`id`) and engine may perform an addition document fetching by its primary key to finish query matching. So for not so large collections a brute scan may perform better than scan using indexes.

However, exact matching operations: `eq`, `in` and `sorting` by natural index order will always benefit from index in any case.



# HTTP REST/Websocket API endpoint

EJDB engine provides the ability to start a separate HTTP/Websocket endpoint worker exposing network API for quering and data modifications.

The easiest way to expose database over the network is using the standalone `jbs` server. (Of course if you plan to avoid `C API` integration).

## jbs server

```
jbs -h

EJDB 2.0.0 standalone REST/Websocket server. http://ejdb.org

 --file <>	Database file path. Default: db.jb
 -f <>    	(same as --file)
 --port ##	HTTP port number listen to. Default: 9191
 -p ##    	(same as --port)
 --bind <>	Address server listen. Default: localhost
 -b <>    	(same as --bind)
 --access <>	Server access token matched to 'X-Access-Token' HTTP header value
 -a <>      	(same as --access)
 --trunc   	Cleanup existing database file on open
 -t        	(same as --trunc)
 --wal   	Use write ahead logging (WAL). Must be set for data durability.
 -w      	(same as --wal)

Advanced options
 --sbz ##	Max sorting buffer size. If exeeded, an overflow temp file for data will created. Default: 16777216, min: 1048576
 --dsz ##	Initial size of buffer to process/store document on queries. Preferable average size of document. Default: 65536, min: 16384
 --bsz ##	Max HTTP/WS API document body size. Default: 67108864, min: 524288

Use any of the following input formats:
	-arg <value>	-arg=<value>	-arg<value>

Use the -h, -help or -? to get this information again.
```

## HTTP API

Access to HTTP endpoint can be protected by a token specified with `--access`
command flag or by C API `EJDB_HTTP` options. If access token specified on server, a client should provide `X-Access-Token` HTTP header value. If token is required and not provided by client `401` HTTP code will be returned. If access token is required and not matched to the token provided by client `403` HTTP code will returned. In any error case `500` error will be returned.

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
* `404` if document not found

### PATCH /{collection}/{id}
Patch a document identified by `id` by [rfc7396](https://tools.ietf.org/html/rfc7396),
[rfc6902](https://tools.ietf.org/html/rfc6902) data.
* `200` on success. Empty body

### GET | HEAD /{collections}/{id}
Retrieve document identified by `id` from a `collection`.
* `200` on success. Body: JSON document text.
  * `content-type:application/json`
  * `content-length:`
* `404` if document not found

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
wscat  -H 'X-Access-Token:myaccess01' -q -c http://localhost:9191
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

Note about `<key>` prefix before every command; It is an arbitrary key choosen by client and designated to identify particular websocket request, this key will be returned with response to request and allows client to identify that response for his particular request.

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
Execute query text. Body of query should contains collection name in use in the first filter element: `@collection_name/...`. Behaviour is the same as for: `<key> query   <collection> <query>`

#### `<key> idx     <collection> <mode> <path>`
Ensure index with specified `mode` (bitmask flag) for given json `path` and `collection`.
Collection will be created if not exists.

Index mode | Description
--- | ---
<code>0x01 EJDB_IDX_UNIQUE</code> | Index is unique
<code>0x04 EJDB_IDX_STR</code> | Index for JSON `string` field value type
<code>0x08 EJDB_IDX_I64</code> | Index for `8 bytes width` signed integer field values
<code>0x10 EJDB_IDX_F64</code> | Index for `8 bytes width` signed floating point field values.

Example:
```
k idx mycollection 5 /name
```

#### `<key> rmi     <collection> <mode> <path>`
Remove index with specified `mode` (bitmask flag) for given json `path` and `collection`.
Return error if given index not found.

#### `<key> rmc     <collection>`
Remove collection and all of its data.
Note: If `collection` is not found no errors will be reported.




# C API

EJDB can be empdedded into any `C/C++` application.
`C API` documented in the following headers:

* [ejdb.h]() Main API functions
* [jbl.h]() JSON documents management API
* [jql.h]() Query building API

Example application:
```c
#include <ejdb2.h>

#define RCHECK(rc_)          \
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
  RCHECK(rc);

  rc = ejdb_open(&opts, &db);
  RCHECK(rc);

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
  if (q) jql_destroy(&q);
  if (jbl) jbl_destroy(&jbl);
  ejdb_close(&db);
  RCHECK(rc);
  return 0;
}
```

# License
```

MIT License

Copyright (c) 2012-2019 Softmotions Ltd <info@softmotions.com>

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

