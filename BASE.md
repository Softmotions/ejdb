# EJDB 2.0

![license](https://img.shields.io/github/license/Softmotions/ejdb.svg)
![maintained](https://img.shields.io/maintenance/yes/2019.svg)
<a href="https://twitter.com/ejdblab">
        <img src="https://img.shields.io/twitter/follow/ejdblab.svg?style=social"
            alt="follow on Twitter"></a>

EJDB2 is an embeddable JSON database engine published under MIT license.

* C11 API
* Single file database
* Simple but powerful query language (JQL) as well as support of the following standards:
  * [rfc6902](https://tools.ietf.org/html/rfc6902) JSON Patch
  * [rfc7386](https://tools.ietf.org/html/rfc7386) JSON Merge patch
  * [rfc6901](https://tools.ietf.org/html/rfc6901) JSON Path
* Powered by [iowow.io](http://iowow.io) - The persistent key/value storage engine
* Provides HTTP REST/Websockets network endpoints with help of [facil.io](http://facil.io)
* JSON documents are stored in using fast and compact [binn](https://github.com/liteserver/binn) binary format


## EJDB2 status

* EJDB 2.0 is currently in Beta state, stable enough but some gotchas may be arised.
* Tested on `Linux` platform, still pending tests for `OSX`.
* `Windows` platform not supported at now (#237)
* Old EJDB 1.x version can be found in separate [ejdb_1.x](https://github.com/Softmotions/ejdb/tree/ejdb_1.x) branch.
  We are not maintaining ejdb 1.x.

# Supported platforms

## Linux
### Ubuntu/Debian
#### PPA repository

```sh
sudo add-apt-repository ppa:adamansky/ejdb2
sudo apt-get update
sudo apt-get install ejdb2
```

#### Building debian packages

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_DEB=ON
make package
```

### RPM based Linux distributions
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_RPM=ON
make package
```
