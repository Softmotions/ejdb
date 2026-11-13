# HTTP REST/WebSocket API

EJDB2 can expose its data over the network:

* **REST API** over HTTP.
* **WebSocket API** for interactive/persistent sessions.
* **TLS** (HTTP and WebSocket) with a PEM key and certificate.

The endpoint can be started either by the standalone `jbs` server or embedded into
an application through `struct ejdb_http` / `jbr.h` (see
[Embedded HTTP server](#embedded-http-server)).

All document payloads are JSON; queries are written in
[JQL](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md).

## `jbs` server

```
Usage: jbs [options]

  -f, --file=<>        Database file path. Default: ejdb2.db
  -p, --port=NUM       HTTP server port number. Default: 9191
  -l, --listen=<>      Network address to listen on. Default: localhost
  -k, --key=<>         PEM private key file for TLS 1.2
  -c, --certs=<>       PEM certificates file for TLS 1.2
  -a, --access=TOKEN   Access token expected in the X-Access-Token header
  -r, --access-read    Allow anonymous read-only access
  -C, --cors           Enable CORS response headers
  -t, --trunc          Cleanup/reset the database file on open
  -w, --wal            Enable the write-ahead log (WAL)

Advanced options:
  -S, --sbz=NUM        Max sorting buffer size, default: 16777216, min: 1048576
  -D, --dsz=NUM        Initial buffer size for documents processed during queries,
                       default: 65536, min: 16384
  -T, --trylock        Exit with an error if the database is locked by another
                       process instead of waiting for the lock
```

Notes:

* `-b, --bind` is accepted as an alias of `-l, --listen` for backward compatibility.
* `jbs` disables the write-ahead log by default; `-w` turns it on. The embedded C API
  keeps WAL enabled unless `struct ejdb_opts.no_wal` is set.
* The access token is used as is; it is not read from a file.

Examples:

```sh
# Local server protected by an access token
./jbs -a 'myaccess01'

# Listen on all interfaces, allow anonymous reads, enable CORS
./jbs -l 0.0.0.0 -a 'myaccess01' -r -C

# TLS endpoint
./jbs -a 'myaccess01' -k server.key -c server.crt
```

## Access control

When an access token is configured (`-a` or `struct ejdb_http.access_token`), every
HTTP request and every WebSocket handshake must provide it in the `X-Access-Token`
header.

Request | Result
--- | ---
Token missing, token required | `401 Unauthorized`
Token provided but does not match | `403 Forbidden`
Token valid | Request is processed

With `-r` (`--access-read`, `read_anon = true`) an **anonymous** client (no token) may
perform read requests only:

* `GET`/`HEAD /{collection}/{id}` and `POST /` (query) are allowed.
* A query that contains an `apply`/`upsert`/`del` section is rejected with `403`.
* Any mutating request (`POST /{collection}`, `PUT`, `PATCH`, `DELETE`) is rejected
  with `401`.
* Over WebSocket, `info` and all mutating commands are rejected with
  `Access denied (JBR_ERROR_WS_ACCESS_DENIED)`.
* `OPTIONS /` is not available anonymously (`401`).

A caller that provides a **valid** token is not limited by `-r`; the flag affects only
anonymous requests.

## Embedded HTTP server

The same endpoint can be started by the `ejdb2` library. Fill
`struct ejdb_opts.http` and set `enabled = true`:

```c
struct ejdb_opts opts = {
  .kv = { .path = "example.db" },
  .http = {
    .enabled = true,
    .port = 9191,
    .bind = "localhost",           // Default: "localhost"
    .access_token = "myaccess01",  // Default: zero (no token required)
    .access_token_len = 10,
    .read_anon = true,             // Allow anonymous read-only access
    .cors = true,                  // Send CORS headers
    .blocking = false,             // false: serve in a background thread
    .ssl_private_key = "server.key",
    .ssl_certs = "server.crt",
    .max_body_size = 0             // Default: 64Mb, minimum: 512K
  }
};
struct ejdb *db;
iwrc rc = ejdb_open(&opts, &db);
```

Behaviour:

* With `blocking = false` the server runs in a background thread and `ejdb_open()`
  returns immediately.
* With `blocking = true` `ejdb_open()` enters the server loop and returns only after
  the server is stopped (this is what `jbs` does).
* `ejdb_close()` stops the embedded server and waits for it to finish.
* `jbr.h` exposes the server lifecycle directly for applications that need it:
  `jbr_start()`, `jbr_shutdown_request()`, `jbr_shutdown_wait()`, `jbr_init()`.

## HTTP REST API

### Common rules

* A collection name is a non-empty string of at most `EJDB_COLLECTION_NAME_MAX_LEN`
  (255) bytes.
* A document identifier is a positive `int64` (`>= 1`).
* Request paths are `/{collection}` or `/{collection}/{id}`.
  `{id}` must be a decimal integer; `0`, a negative value, or a non-numeric value is
  rejected with `400`.
* Missing or unparsable JSON bodies, invalid queries, and malformed paths produce
  `400 Bad Request`.
* Unknown documents produce `404 Not Found`.
* Internal failures produce `500`.
* Error responses have a `text/plain` body with a human readable description
  (for example `Not Found` or `Failed to parse JSON string (JBL_ERROR_PARSE_JSON)`).

Responses to authorized requests include `Access-Control-Allow-Origin: *` when CORS
is enabled. Authentication failures (`401`/`403`) are produced before the request is
routed and do not carry CORS headers.

### POST `/{collection}`

Add a new document to `{collection}`. The collection is created if it does not exist.

* Request body: JSON document.
* `200 OK`, `text/plain` body: the new document identifier as an `int64` number.

```sh
curl -s -X POST --data-raw '{"firstName":"John","age":28}' \
     -H 'X-Access-Token: myaccess01' http://localhost:9191/family
1
```

### GET | HEAD `/{collection}/{id}`

Retrieve a document.

* `200 OK`, `application/json` body: the document, pretty printed.
* `404 Not Found` if the document (or the collection) does not exist.
* `HEAD` returns the same status and headers, including `Content-Length`, without a body.
* `400` when the identifier is missing (`GET /{collection}`).

```sh
curl -s -H 'X-Access-Token: myaccess01' http://localhost:9191/family/1
{
 "firstName": "John",
 "age": 28
}
```

### PUT `/{collection}/{id}`

Store a document under the given numeric identifier, replacing an existing document.
The collection is created if it does not exist.

* Request body: JSON document.
* `200 OK`, `text/plain` body: `OK`.
* `400` for an invalid body or identifier.

### PATCH `/{collection}/{id}`

Patch a document with an [RFC 7396](https://tools.ietf.org/html/rfc7396) merge patch
(a JSON object) or an [RFC 6902](https://tools.ietf.org/html/rfc6902) JSON patch
(a JSON array).

* `200 OK`, `text/plain` body: `OK`.
* `400` for an invalid patch.
* `404` if the document is not found.

### DELETE `/{collection}/{id}`

Remove a document.

* `200 OK`, `text/plain` body: `OK`.
* `404` if the document is not found.
* Requires a writable (token-authenticated) client when `-r` is used.

### POST `/`

Execute a [JQL](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md)
query. The request body is the query text; the collection must be encoded in the
query with an `@collection` anchor.

* Request header `X-Hints` — comma separated hints:
  * `explain` — print the query execution plan before the results, separated by a
    `--------------------` line.
* Response is streamed with
  [HTTP chunked transfer encoding](https://en.wikipedia.org/wiki/Chunked_transfer_encoding).
* `200 OK`; each matched document is sent as:

  ```
  \r\n<document id>\t<document JSON>
  ```

  There is no trailing newline after the last document. A query that matches nothing
  returns an empty body.
* A query with the `count` option returns the number of matched documents as
  `text/plain` instead of documents.
* `400` for an empty body, a query without a collection anchor
  (`No collection specified in query (JQL_ERROR_NO_COLLECTION)`), or a query parse
  error (the parse error text is returned).
* `403` for an `apply`/`upsert`/`del` query sent by an anonymous client in read-only
  mode.

```sh
curl -s -X POST --data-raw '@family/[age > 18]' \
     -H 'X-Access-Token: myaccess01' http://localhost:9191/

2	{"firstName":"Jack","age":35}
1	{"firstName":"John","age":28}
```

With the `explain` hint:

```sh
curl -s -X POST --data-raw '@family/[age > 18]' \
     -H 'X-Access-Token: myaccess01' -H 'X-Hints: explain' http://localhost:9191/
[INDEX] NO [COLLECTOR] PLAIN
--------------------
2	{"firstName":"Jack","age":35}
1	{"firstName":"John","age":28}
```

Counting without transferring documents:

```sh
curl -s -X POST --data-raw '@family/* | count' \
     -H 'X-Access-Token: myaccess01' http://localhost:9191/
2
```

### OPTIONS `/`

Fetch database metadata as a JSON document. The response also contains the `Allow`
header with the methods accepted by the endpoint, and CORS headers when CORS is enabled.

```sh
curl -s -X OPTIONS -H 'X-Access-Token: myaccess01' http://localhost:9191/
{
 "version": "2.100",
 "file": "/var/lib/ejdb2/example.db",
 "size": 32768,
 "collections": [
  {
   "name": "family",
   "dbid": 3,
   "rnum": 2,
   "indexes": []
  }
 ]
}
```

The `Allow` header lists `GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS` for a writable
endpoint, and `GET, HEAD, POST, OPTIONS` when the server runs in read-only mode
(`-r`).

`OPTIONS /{collection}` is not supported (`400`).

## WebSocket API

The WebSocket endpoint is the server root (`GET /` with `Upgrade: websocket` on the
same port). If an access token is configured it must be sent in the
`X-Access-Token` header of the handshake.

The protocol is text based: every WebSocket text message contains one command, and a
command may produce one or more response messages. Messages are tab separated.

### Command format

```
<key> <command> [arguments]
```

`<key>` is an arbitrary client-chosen token of at most 36 bytes (not containing
whitespace). It is echoed back as the first field of every response message, so a
client can match responses to requests.

```
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
```

Send `?` (a message consisting of a single question mark) to receive this help text.

### Response format

Response | Meaning
--- | ---
`<key>\t<metadata json>` | `info` result
`<key>\t<id>` | `add`, `set`, `del`, `patch` result
`<key>\t<id>\t<document json>` | `get` result, or one query result document
`<key>\t<explain>\t<plan>` | query execution plan (`explain`)
`<key>\t<count>` | `count` query result
`<key>` | End of a query result set. Also the successful reply of `idx`, `rmi`, `rmc`
`<key> ERROR: <description>` | Error

Examples below are shown as rendered by
[wscat](https://www.npmjs.com/package/@softmotions/wscat); response fields are
separated by horizontal tabs.

### `?`

Request the list of supported commands:

```sh
wscat -H 'X-Access-Token: myaccess01' -c http://localhost:9191
> ?
< <key> info
< <key> get     <collection> <id>
...
```

### `<key> info`

Return database metadata as a pretty printed JSON document.

```sh
> k info
< k	{
 "version": "2.100",
 "file": "/tmp/ejdb2.db",
 "size": 32768,
 "collections": [ ... ]
}
```

Anonymous clients are not allowed to call `info` in read-only mode.

### `<key> add <collection> <document json>`

Add a new document and return its generated identifier. The collection is created if
it does not exist.

```sh
> k add mycollection {"foo":"bar"}
< k	1
> k add mycollection {"foo":"bar"}
< k	2
```

### `<key> set <collection> <id> <document json>`

Store a document under the given numeric identifier, replacing an existing document.

```sh
> k set mycollection 3 {"foo":"baz"}
< k	3
```

### `<key> get <collection> <id>`

Return a document. The document is pretty printed.

```sh
> k get mycollection 1
< k	1	{
 "foo": "bar"
}
```

If the document is not found:

```sh
> k get mycollection 55
< k ERROR: Key not found. (IWKV_ERROR_NOTFOUND)
```

### `<key> del <collection> <id>`

Remove a document. Returns its identifier, or an error if the document is not found.

### `<key> patch <collection> <id> <patch json>`

Apply an RFC 7396 merge patch (JSON object) or an RFC 6902 JSON patch (JSON array) to
the document. Returns the document identifier.

### `<key> query <collection> <query>`

Execute a JQL query. Each matched document is sent as a separate message, and the
result set is terminated by a message containing only the key:

```sh
> k query family /* | /firstName
< k	3	{"firstName":"John"}
< k	2	{"firstName":"Jack"}
< k	1	{"firstName":"John"}
< k
```

A query with the `count` option returns a single message with the number of matched
documents and no termination message:

```sh
> k query family /* | count
< k	3
```

### `<key> explain <collection> <query>`

Same as `query`, but sends the execution plan first:

```sh
> k explain family /* | /firstName
< k	explain	[INDEX] NO [COLLECTOR] PLAIN
< k	3	{"firstName":"John"}
< k	2	{"firstName":"Jack"}
< k	1	{"firstName":"John"}
< k
```

### `<key> <query>`

Execute the query text given in the message. The collection must be encoded in the
query with an `@collection` anchor:

```sh
> k @family/[age > 30]
< k	2	{"firstName":"Jack","age":35}
< k
```

### `<key> idx <collection> <mode> <path>`

Create an index for the given JSON path. Returns an empty message on success. The
collection is created if it does not exist.

Index mode | Description
--- | ---
<code>0x01 EJDB_IDX_UNIQUE</code> | Index is unique
<code>0x04 EJDB_IDX_STR</code> | Index for JSON `string` values
<code>0x08 EJDB_IDX_I64</code> | Index for 64 bit signed integer values
<code>0x10 EJDB_IDX_F64</code> | Index for floating point values

For example a unique string index over `/name` uses mode `0x01 | 0x04 = 5`:

```sh
> k idx mycollection 5 /name
< k
```

### `<key> rmi <collection> <mode> <path>`

Remove an index. Returns an empty message on success, including the case when the
index does not exist. A missing collection is reported as
`Resource is not exists. (IW_ERROR_NOT_EXISTS)`.

### `<key> rmc <collection>`

Remove a collection with all its documents and indexes. Removing a missing collection
is not an error.

### Errors and malformed messages

Errors are returned as:

```
<key> ERROR: <description>
```

For example:

```sh
> k get mycollection 55
< k ERROR: Key not found. (IWKV_ERROR_NOTFOUND)
```

A message that cannot be parsed far enough to read the key (for example a lone `<key>`
without a command) terminates the WebSocket session.

## See also

* [JQL query language](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md)
* [C API](https://github.com/Softmotions/ejdb/blob/master/CAPI.md)
* [Test suite and tools](https://github.com/Softmotions/ejdb/blob/master/TESTS.md)
