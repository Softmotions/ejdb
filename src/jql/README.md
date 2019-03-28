# JQL

EJDB query (JQL) syntax inpired by ideas behind XPath and Unix shell pipes.
It designed to easy filtering single collection of json documents.

## JQL grammar

Formal JQL grammar is part of project and created with
[peg/leg — recursive-descent parser generators for C](http://piumarta.com/software/peg/):

https://github.com/Softmotions/ejdb/blob/ejdb2/src/jql/jqp.leg

## Non formal JQL grammar adapted for brief overview

Notation is based on SQL syntax notation:

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
* `json_path`: Simplified JSON pointer. Eg.: `/foo/bar` or `/foo/"bar with space"/`
* `*` in context of `NODE`: Any JSON object key name at particular nesting level.
* `**` in context of `NODE`: Any JSON object key name at arbitrary nesting level.
* `*` in context of `NODE_EXPR_LEFT`: Key name at specific level.
* `**` in context of `NODE_EXPR_LEFT`: Nested array value of array element under specific key.

## JQL cookbook

### Sample 1

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
Save it into `sample.json` then upload to server:

```sh
# Start server with access token
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
   "name": "Rex",
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

#### Get all elements in collection
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
HTTP protocol
```
curl --data-raw '@family/*' -H'X-Access-Token:myaccess01' -X POST http://localhost:9191

1	{"firstName":"John","lastName":"Doe","age":28,"pets":[{"name":"Rex","kind":"dog","likes":["bones","jumping","toys"]},{"name":"Grenny","kind":"parrot","likes":["green color","night","toys"]}]}
```

##### Set max number of elements in result set

```
k @family/* | limit 10
```

#### Get documents where specific json path exists

Exists element at index `1` in `likes` array within a `pets` sub-object
```
> k query family /pets/*/likes/1
< k     1       {"firstName":"John"...
```

Exists first element in `likes` array at any nesting level
```
> k query family /**/likes/1
< k     1       {"firstName":"John"...
```











