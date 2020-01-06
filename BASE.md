# EJDB 2.0

[The Story of the IT-depression, birds and EJDB 2.0](https://medium.com/@adamansky/ejdb2-41670e80897c)

[![Join ejdb2 telegram](https://img.shields.io/badge/join-ejdb2%20telegram-0088cc.svg)](https://t.me/ejdb2)
[![license](https://img.shields.io/github/license/Softmotions/ejdb.svg)](https://github.com/Softmotions/ejdb/blob/master/LICENSE)
![maintained](https://img.shields.io/maintenance/yes/2019.svg)


EJDB2 is an embeddable JSON database engine published under MIT license.

* C11 API
* Single file database
* Online backups support
* 500K library size for Android
* [iOS](#ejdb2swift) / [Android](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_android/test) / [React Native](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_react_native) / [Flutter](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_flutter) integration
* Simple but powerful query language (JQL) as well as support of the following standards:
  * [rfc6902](https://tools.ietf.org/html/rfc6902) JSON Patch
  * [rfc7386](https://tools.ietf.org/html/rfc7386) JSON Merge patch
  * [rfc6901](https://tools.ietf.org/html/rfc6901) JSON Path
* Powered by [iowow.io](http://iowow.io) - The persistent key/value storage engine
* Provides HTTP REST/Websockets network endpoints with help of [facil.io](http://facil.io)
* JSON documents are stored in using fast and compact [binn](https://github.com/liteserver/binn) binary format

---
* [Native language bindings](#native-language-bindings)
* Supported platforms
  * [OSX](#osx)
  * [iOS](#ejdb2swift)
  * [Linux](#linux)
  * [Android](#android)
  * [Windows](#windows)
* [JQL query language](#jql)
  * [Grammar](#jql-grammar)
  * [Quick into](#jql-quick-introduction)
  * [Data modification](#jql-data-modification)
  * [Projections](#jql-projections)
  * [Sorting](#jql-sorting)
  * [Query options](#jql-options)
* [Indexes and performance](#jql-indexes-and-performance-tips)
* [Network API](#http-restwebsocket-api-endpoint)
  * [HTTP API](#http-api)
  * [Websockets API](#websocket-api)
* [C API](#c-api)
* [License](#license)
---

## Native language bindings

* Node.js https://www.npmjs.com/package/ejdb2_node
* Dart https://pub.dartlang.org/packages/ejdb2_dart
* Java [ejdb2_jni/README.md](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_jni/README.md)
* Android support (see below)
* [React Native](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_react_native)
* [Flutter](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_flutter)

## Status

* **EJDB 2.0 core engine is well tested and used in various heavily loaded deployments**
* Tested on `Linux` and `OSX` platforms. [Limited Windows support](./WINDOWS.md)
* Old EJDB 1.x version can be found in separate [ejdb_1.x](https://github.com/Softmotions/ejdb/tree/ejdb_1.x) branch.
  We are not maintaining ejdb 1.x.

## Use cases

* Softmotions trading robots platform

# Supported platforms

## OSX

EJDB2 code ported and tested on `High Sierra` / `Mojave` / `Catalina`

See also [EJDB2 Swift binding](#ejdb2swift) for OSX, iOS and Linux

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## Linux
### Ubuntu/Debian
#### PPA repository

```sh
sudo add-apt-repository ppa:adamansky/ejdb2
sudo apt-get update
sudo apt-get install ejdb2
```

#### Building debian packages

cmake v3.15 or higher required

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_DEB=ON
make package
```

#### RPM based Linux distributions
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPACKAGE_RPM=ON
make package
```

## Windows
EJDB2 can be cross-compiled for windows

**Note:** HTTP/Websocket network API is disabled and not supported
on Windows until port of http://facil.io library (#257)

Nodejs/Dart bindings not yet ported to Windows.

**[Cross-compilation Guide for Windows](./WINDOWS.md)**
