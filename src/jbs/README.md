# Standalone REST/Websocket HTTP server

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
