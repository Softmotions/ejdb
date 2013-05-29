Embedded JSON database library Python 2.7/3.x binding
============================================================

Installation
---------------------------------


**Required tools/system libraries:**

* gcc
* **Python3 >= 3.2 or Python2: 2.7.x**
* EJDB C library **libtcejdb** ([from sources](https://github.com/Softmotions/ejdb#manual-installation) or as [debian packages](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation))

**(A) Using pip**

`pip` for python3 or python2 should be installed (`sudo apt-get install python3-pip` | `sudo apt-get install python-pip`)

```
   sudo pip install pyejdb

Upgrading:
   sudo pip install pyejdb --upgrade
```

**(B) Installing directly from sources**

```
git clone https://github.com/Softmotions/ejdb.git
cd ./pyejdb
sudo python3 ./setup.py install
```


**(C) Installing on Ubuntu/Debian (only for Python3.2)**

```
sudo add-apt-repository ppa:adamansky/ejdb
sudo apt-get update
sudo apt-get install python3-ejdb
```


One snippet intro
---------------------------------

```python
import pyejdb
from datetime import datetime

#Open database
ejdb = pyejdb.EJDB("zoo", pyejdb.DEFAULT_OPEN_MODE | pyejdb.JBOTRUNC)

parrot1 = {
    "name": "Grenny",
    "type": "African Grey",
    "male": True,
    "age": 1,
    "birthdate": datetime.utcnow(),
    "likes": ["green color", "night", "toys"],
    "extra1": None
}
parrot2 = {
    "name": "Bounty",
    "type": "Cockatoo",
    "male": False,
    "age": 15,
    "birthdate": datetime.utcnow(),
    "likes": ["sugar cane"],
    "extra1": None
}
ejdb.save("parrots2", parrot1, parrot2)

with ejdb.find("parrots2", {"likes" : "toys"},
          hints={"$orderby" : [("name", 1)]}) as cur:
    print("found %s parrots" % len(cur))
    for p in cur:
        print("%s likes toys!" % p["name"])

ejdb.close()
```

