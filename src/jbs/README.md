# REST/Websocket HTTP server.

SSL (TLS 1.2) provided by fork of BearSSL library https://github.com/Softmotions/BearSSL

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
