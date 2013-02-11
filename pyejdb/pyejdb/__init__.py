import _pyejdb
from pprint import pprint
from pyejdb import bson

__all__ = [
    "EJDB"
]

def _check_collname(cname):
    if not isinstance(cname, str):
        raise TypeError("Collection name must be an instance of %s" % str.__name__)


class EJDB(object):
    def __init__(self, fpath):
        #pprint (vars(_pyejdb))
        self.__ejdb = _pyejdb.EJDB()
        self.__ejdb.open(fpath, _pyejdb.JBOWRITER | _pyejdb.JBOCREAT | _pyejdb.JBOTSYNC)

    def close(self):
        if self.__ejdb:
            self.__ejdb.close()
            self.__ejdb = None

    def save(self, cname, *jsarr, **kwargs):
        _check_collname(cname)
        for doc in jsarr:
            _oid = self.__ejdb.save(cname, bson.serialize_to_bytes(doc), **kwargs)
            if "_id" not in doc:
                doc["_id"] = _oid
                print("ID=%s" % _oid)









