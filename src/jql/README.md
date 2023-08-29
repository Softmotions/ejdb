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
