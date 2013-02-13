import _pyejdb
from functools import lru_cache
from pprint import pprint
from pyejdb import bson
from pyejdb.typecheck import *
import re
import numbers

__all__ = [

    "EJDB",

    "JBOREADER",
    "JBOWRITER",
    "JBOCREAT",
    "JBOTRUNC",
    "JBONOLCK",
    "JBOLCKNB",
    "JBOTSYNC",
    "DEFAULT_OPEN_MODE",

    "JBQRYCOUNT",

    "check_oid",
    "version",
    "libejdb_version"
]

version_tuple = (1, 0, 1)

def get_version_string():
    return '.'.join(map(str, version_tuple))

version = get_version_string()
libejdb_version = _pyejdb.ejdb_version()

#OID check RE
_oidRE = re.compile("^[0-9a-f]{24}$")

#Open modes
JBOREADER = _pyejdb.JBOREADER
JBOWRITER = _pyejdb.JBOWRITER
JBOCREAT = _pyejdb.JBOCREAT
JBOTRUNC = _pyejdb.JBOTRUNC
JBONOLCK = _pyejdb.JBONOLCK
JBOLCKNB = _pyejdb.JBOLCKNB
JBOTSYNC = _pyejdb.JBOTSYNC
DEFAULT_OPEN_MODE = JBOWRITER | JBOCREAT | JBOTSYNC

#Query flags
JBQRYCOUNT = _pyejdb.JBQRYCOUNT

#Misc
def check_oid(oid):
    if not isinstance(oid, str) or _oidRE.match(oid) is None:
        raise ValueError("Invalid OID: %s" % oid)


class EJDBCursorWrapper(object):
    def __init__(self, dbcursor):
        self.__pos = 0
        self.__cursor = dbcursor
        self.__len = self.__cursor.length()

    def __del__(self):
        self.__cursor.close()

    def __len__(self):
        return self.__len

    def __getitem__(self, key):
        if isinstance(key, slice):
            return [self[idx] for idx in range(*key.indices(len(self)))]
        elif isinstance(key, numbers.Number):
            if key < 0:
                key += len(self)
            if key >= len(self):
                raise IndexError("The index (%d) is out of range." % key)
            return self.get(key)
        else:
            raise TypeError("Invalid argument type.")

    def __iter__(self):
        return self

    def __next__(self):
        if self.__pos >= self.__len:
            raise StopIteration
        self.__pos += 1
        return self.get(self.__pos - 1)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def reset(self):
        self.__pos = 0

    @lru_cache(maxsize=8192)
    def get(self, idx):
        bsdata = self.__cursor.get(idx)
        return bson.parse_bytes(bsdata) if bsdata is not None else None

    def close(self):
        self.__cursor.close()


class EJDB(object):
    def __init__(self, fpath, mode=DEFAULT_OPEN_MODE):
        #pprint (vars(_pyejdb))
        self.__ejdb = _pyejdb.EJDB()
        self.__ejdb.open(fpath, mode)

    @property
    def isopen(self):
        return self.__ejdb.isopen()

    def close(self):
        return self.__ejdb.close()

    def sync(self):
        return self.__ejdb.sync()

    @typecheck
    def save(self, cname : str, *jsarr, **kwargs):
        for doc in jsarr:
            _oid = self.__ejdb.save(cname, bson.serialize_to_bytes(doc), **kwargs)
            if "_id" not in doc:
                doc["_id"] = _oid

    @typecheck
    def load(self, cname : str, oid : str):
        check_oid(oid)
        docbytes = self.__ejdb.load(cname, oid)
        if docbytes is None:
            return None
        return bson.parse_bytes(docbytes)

    @typecheck
    def remove(self, cname : str, oid):
        check_oid(oid)
        return self.__ejdb.remove(cname, oid)

    @typecheck
    def find(self, cname : str, qobj : optional(dict)=None, *args, **kwargs):
        if not qobj: qobj = {}
        qobj = bson.serialize_to_bytes(qobj)
        hints = bson.serialize_to_bytes(kwargs.get("hints", {}))
        orarr = [bson.serialize_to_bytes(x) for x in args]
        qflags = kwargs.get("qflags", 0)
        cursor = self.__ejdb.find(cname, qobj, orarr, hints, qflags)
        return cursor if isinstance(cursor, numbers.Number) else EJDBCursorWrapper(cursor)

    def findOne(self, cname : str, qobj : optional(dict)=None, *args, **kwargs):
        hints = kwargs.get("hints", {})
        hints["$max"] = 1
        kwargs["hints"] = hints
        with self.find(cname, qobj, *args, **kwargs) as res:
            return res[0] if len(res) > 0 else None

    def update(self, cname : str, qobj : optional(dict)=None, *args, **kwargs):
        qflags = kwargs.get("qflags", 0)
        kwargs["qflags"] = qflags | JBQRYCOUNT
        return self.find(cname, qobj, *args, **kwargs)

    def count(self, cname : str, qobj : optional(dict)=None, *args, **kwargs):
        qflags = kwargs.get("qflags", 0)
        kwargs["qflags"] = qflags | JBQRYCOUNT
        return self.find(cname, qobj, *args, **kwargs)

    @typecheck
    def ensureCollection(self, cname : str, **kwargs):
        return self.__ejdb.ensureCollection(cname, **kwargs)

    @typecheck
    def dropCollection(self, cname : str, **kwargs):
        return self.__ejdb.dropCollection(cname, **kwargs)


