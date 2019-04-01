# EJDB 2.0

![license](https://img.shields.io/github/license/Softmotions/ejdb.svg) ![maintained](https://img.shields.io/maintenance/yes/2019.svg)


EJDB2 is an embeddable JSON database engine published under MIT license.

* C11 API
* Simple but powerful query language (JQL) as well as support of the following standards:
  * [rfc6902](https://tools.ietf.org/html/rfc6902) JSON Patch
  * [rfc7386](https://tools.ietf.org/html/rfc7386) JSON Merge patch
  * [rfc6901](https://tools.ietf.org/html/rfc6901) JSON Path
* Based on [iowow.io](http://iowow.io) persistent key/value storage engine
* Provides HTTP REST/Websockets network endpoints implemented with [facil.io](http://facil.io)
* JSON documents are stored in using fast and compact binary format [Binn](https://github.com/liteserver/binn)


## EJDB2 status

* EJDB 2.0 is currently in Beta state, stable enough but some gotchas may be arised.
* Tested on `Linux` platform, still pending tests for `OSX`.
* `Windows` platform not supported at now (#237)
* Old EJDB 1.x version can be found in separate [ejdb_1.x](https://github.com/Softmotions/ejdb/tree/ejdb_1.x) branch.
  We are not maintaining ejdb 1.x.

