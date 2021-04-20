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
 --sbz ##	Max sorting buffer size. If exceeded, an overflow temp file for data will be created. Default: 16777216, min: 1048576
 --dsz ##	Initial size of buffer to process/store document on queries. Preferable average size of document. Default: 65536, min: 16384
 --bsz ##	Max HTTP/WS API document body size. Default: 67108864, min: 524288

Use any of the following input formats:
	-arg <value>	-arg=<value>	-arg<value>

Use the -h, -help or -? to get this information again.
```

## HTTP API

Access to HTTP endpoint can be protected by a token specified with `--access`
command flag or by C API `EJDB_HTTP` options. If access token specified on server, client must provide `X-Access-Token` HTTP header value. If token is required and not provided by client the `401` HTTP code will be reported. If access token is not matched to the token provided the `403` HTTP code will be returned.
For any other errors server will respond with `500` error code.

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

