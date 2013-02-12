import _pyejdb
from pprint import pprint
from collections import OrderedDict as odict
from pyejdb import bson
from pyejdb.typecheck import *
import re

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

    "check_oid"
]

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


def check_oid(oid):
    if not isinstance(oid, str) or _oidRE.match(oid) is None:
        raise ValueError("Invalid OID: %s" % oid)


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
        hints = bson.serialize_to_bytes(kwargs["hints"] if "hints" in kwargs else {})
        orarr = [bson.serialize_to_bytes(x) for x in args]
        return self.__ejdb.find(cname, qobj, orarr, hints)
















