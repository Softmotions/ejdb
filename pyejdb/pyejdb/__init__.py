import _pyejdb
from pprint import pprint
from pyejdb import bson
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

def check_collname(cname):
    if not isinstance(cname, str):
        raise TypeError("Collection name must be an instance of %s" % str.__name__)


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
        if self.__ejdb:
            self.__ejdb.close()
            self.__ejdb = None

    def save(self, cname, *jsarr, **kwargs):
        check_collname(cname)
        for doc in jsarr:
            _oid = self.__ejdb.save(cname, bson.serialize_to_bytes(doc), **kwargs)
            if "_id" not in doc:
                doc["_id"] = _oid

    def load(self, cname, oid):
        check_collname(cname)
        check_oid(oid)
        docbytes = self.__ejdb.load(cname, oid)
        if docbytes is None:
            return None
        return bson.parse_bytes(docbytes)

    def remove(self, cname, oid):
        check_collname(cname)
        check_oid(oid)
        return self.__ejdb.remove(cname, oid)















