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

## HTTP API access

Access to HTTP endpoint can be protected by a token specified with `--access`
command flag or by C API `EJDB_HTTP` options. If access token specified on server, a client should provide `X-Access-Token` HTTP header value. If token is required and not provided by client `401` HTTP code will be returned. If access token is required and not matched to the token provided by client `403` HTTP code will returned. In any error case `500` error will be returned.

## REST API

### POST /{collection}
Add a new document to the `collection`.
* `200` success, body: a new document identifier as `int64` number

### PUT /{collection}/{id}
Replaces/store document under specific numeric `id`
* `200` success, empty body

### DELETE /{collection}/{id}
Removes document identified by `id` from a `collection`
* `200` success, empty body
* `404` document not found

### PATCH /{collection}/{id}
Patch a document identified by `id` by [rfc7396](https://tools.ietf.org/html/rfc7396),
[rfc6902](https://tools.ietf.org/html/rfc6902) data.
* `200` success, empty body


