Embedded JSON database library Java binding
============================================================

**[EJDB Java API documentation](http://ejdb.org/javadoc/)**

Installation
---------------------------------

**Required tools/system libraries:**

* gcc
* **Java >= 1.6**
* EJDB C library **libtcejdb** ([from sources](https://github.com/Softmotions/ejdb#manual-installation) or as [debian packages](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation))

**(A) Installing directly from sources**

```
git clone https://github.com/Softmotions/ejdb.git
cd ./jejdb
./configure --prefix=<installation prefix> && make && make tests
sudo make install
```
