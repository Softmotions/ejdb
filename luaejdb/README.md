Embedded JSON database library Lua binding
============================================================

**[EJDB Lua API documentation](http://ejdb.org/luadoc/)**

Installation
-----------------------------------------------------

**Required tools/system libraries:**

* gcc
* **Lua >= 5.1**
* EJDB C library **libtcejdb** ([from sources](https://github.com/Softmotions/ejdb#manual-installation) or as [debian packages](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation))
* [luarocks](http://luarocks.org/en/Download)

**(A) Using luarocks from github sources**

```
umask 022
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb/luaejdb
make
sudo luarocks install ./luaejdb-*.rock
```




