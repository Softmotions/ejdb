import _pyejdb
from pprint import pprint

__all__ = [
    "EJDB"
]

class EJDB(object):
    def __init__(self, fpath):
        #pprint (vars(_pyejdb))
        self.__ejdb = _pyejdb.EJDB()
        self.__ejdb.open(fpath, _pyejdb.JBOWRITER | _pyejdb.JBOCREAT)

    def close(self):
        if self.__ejdb:
            self.__ejdb.close()
            self.__ejdb = None


