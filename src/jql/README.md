# JQL

EJDB query (JQL) syntax inpired by ideas behind XPath and Unix shell pipes.
It designed to easy filter collections of JSON documents.

## JQL grammar

Formal JQL grammar is part of project with parser created by
[peg/leg — recursive-descent parser generators for C](http://piumarta.com/software/peg/)

https://github.com/Softmotions/ejdb/blob/ejdb2/src/jql/jqp.leg

## Non formal JQL grammar adapted for brief overview

Notation is based on SQL syntax description:

Rule | Description
--- | ---
`' '` | String in single quotes denotes unquoted string literal a part of query.
<nobr>`{ a | b }`</nobr> | Curly brackets enclose two or more required alternative choices, separated by vertical bars.
<nobr>`[ ]`</nobr> | Square brackets indicate an optional element or clause. Multiple elements or clauses are separated by vertical bars.
<nobr>`|`</nobr> | Vertical bars separate two or more alternative syntax elements.
<nobr>`...`</nobr> |  Ellipses indicate that the preceding element can be repeated. The repetition is unlimited unless otherwise indicated.
<nobr>`( )`</nobr> | Parentheses are grouping symbols.
Unquoted lower case word | Denotes semantic of some query part. Eg: `placeholder_name` - name of any placeholder.
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

## JQL intro

Lets play with some very basic data and queries.
For simplicity we will use here ejdb websocket network API which provides a kind of interactive CLI.
The same job can be done using pure `C` API (`ejdb2.h`, `jql.h`).

NOTE: Look into [JQL test cases](./tests/jql_test1.c) for more examples.

```json
{
  "firstName": "John",
  "lastName": "Doe",
  "age": 28,
  "pets": [
    {"name": "Rex", "kind": "dog", "likes": ["bones", "jumping", "toys"]},
    {"name": "Grenny", "kind": "parrot", "likes": ["green color", "night", "toys"]}
  ]
}
```
Save json as `sample.json` then upload it the `family` collection:

```sh
# Start server with security access token
./jbs -a 'myaccess01'
8 Mar 16:15:58.601 INFO: HTTP/WS endpoint at localhost:9191
```

Server can be accessed using HTTP or Websocket endpoint. [More info](../jbr/README.md)

```sh
curl -d '@sample.json' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191/family
```

We can play around using [wscat](https://www.npmjs.com/package/@softmotions/wscat) websocket client.

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

Note about the `k` prefix before every command; It is an arbitrary key designated to identify particular websocket request,
this key will be returned with response to this request, as result we can distinct reponses
for different requests over websocket protocol. [More info](../jbr/README.md)

Query command over websocket has the following format:

```
<key> query <collection> <query>
```

So we will consider only `<query>` part here.

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
Note about quotes around words with blanks.

Get all documents where owner `age` greater than `20` and have some pet who like `bones` or `toys`
```
/[age > 20] and /pets/*/likes/[** in ["bones", "toys"]]
```
Here `**` denotes some element in `likes` array.

`ni` is an inverse operator to `in`.
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













