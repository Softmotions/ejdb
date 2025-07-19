# EJDB 2.0

**NOTE: Issues tracker is disabled. You are welcome to contribute, pull requests accepted.**

[![license](https://img.shields.io/github/license/Softmotions/ejdb.svg)](https://github.com/Softmotions/ejdb/blob/master/LICENSE)
![maintained](https://img.shields.io/maintenance/yes/2025.svg)

EJDB2 is an embeddable JSON database engine published under MIT license.

[The Story of the IT-depression, birds and EJDB 2.0](https://medium.com/@adamansky/ejdb2-41670e80897c)

* C11 API
* Single file database
* Online backups support
* [Simple but powerful query language](#jql)
* [Support of collection joins](#jql-collection-joins)
* Powered by [IOWOW](http://iowow.softmotions.com) - The persistent key/value storage engine
* HTTP REST/Websockets endpoints powered by [IWNET](https://github.com/Softmotions/iwnet)

---
* [Native language bindings](#native-language-bindings)
* Supported platforms
  * [macOS](#osx)
  * [iOS](https://github.com/Softmotions/EJDB2Swift)
  * [Linux](#linux)
  * [Windows](#windows)
* **[JQL query language](#jql)**
  * [Grammar](#jql-grammar)
  * [Quick into](#jql-quick-introduction)
  * [Data modification](#jql-data-modification)
  * [Projections](#jql-projections)
  * [Collection joins](#jql-collection-joins)
  * [Sorting](#jql-sorting)
  * [Query options](#jql-options)
* [Indexes and performance](#jql-indexes-and-performance-tips)
* [Network API](#http-restwebsocket-api-endpoint)
  * [HTTP API](#http-api)
  * [Websockets API](#websocket-api)
* [C API](#c-api)
* [License](#license)
---

## Status

* **EJDB 2.0 core engine is well tested and used in various heavily loaded deployments**
* Tested on `Linux`, `macOS` and `FreeBSD`.


## Building from sources 

**Prerequisites**

* Linux, macOS or FreeBSD
* gcc or clang compiler
* pkgconf or pkg-config

**Building**

```sh
./build.sh
```

**Installation**

```sh
./build.sh --prefix=$HOME/.local
```