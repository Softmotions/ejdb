import _pyejdb
from pprint import pprint

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
        self.__ejdb.open(fpath, _pyejdb.JBOWRITER | _pyejdb.JBOCREAT)

    def close(self):
        if self.__ejdb:
            self.__ejdb.close()
            self.__ejdb = None

    def save(self, cname, *jsarr, **kwargs):
        _check_collname(cname)
        _objs = []
        for o in jsarr:
            _objs.append(o)
        _opts = kwargs["opts"] if kwargs["opts"]  else {}





