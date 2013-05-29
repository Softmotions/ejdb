#-*- coding: utf8 -*-

# *************************************************************************************************
#  Python API for EJDB database library http://ejdb.org
#  Copyright (C) 2012-2013 Softmotions Ltd.
#
#  This file is part of EJDB.
#  EJDB is free software; you can redistribute it and/or modify it under the terms of
#  the GNU Lesser General Public License as published by the Free Software Foundation; either
#  version 2.1 of the License or any later version.  EJDB is distributed in the hope
#  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
#  License for more details.
#  You should have received a copy of the GNU Lesser General Public License along with EJDB;
#  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
#  Boston, MA 02111-1307 USA.
# *************************************************************************************************

from __future__ import with_statement
from __future__ import division
from __future__ import print_function

import _pyejdb
import sys
from pyejdb.lrucache import lru_cache
from pyejdb import bson
from collections import OrderedDict as odict
from io import StringIO, BytesIO
import re, numbers


PY3 = sys.version_info[0] == 3

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

version_tuple = (1, 0, 11)

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
    if PY3:
        if not isinstance(oid, str) or _oidRE.match(oid) is None:
            raise ValueError("Invalid OID: %s" % oid)
    else:
        tn = type(oid).__name__
        if (tn != "unicode" and tn != "str") or _oidRE.match(oid) is None:
            raise ValueError("Invalid OID: %s" % oid)


class EJDBCursorWrapper(object):
    """ EJDB resultset cursor returned by `EJDB.find()`.
    Sequences and iterator protocols supported.
    """

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

    def next(self):
        return self.__next__()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def reset(self):
        """ Resets iterator to its initial state.
        """
        self.__pos = 0

    @lru_cache(maxsize=1024)
    def get(self, idx):
        """Return JSON document at the specified position `idx`
        """
        bsdata = self.__cursor.get(idx)
        return bson.parse_bytes_lazy(bsdata) if bsdata is not None else None

    def close(self):
        """ Closes cursor and frees all allocated resources.
        Closed cursor became unusable and its
        methods will raise "Closed cursor" error.
        """
        self.__cursor.close()


class EJDB(object):
    """ EJDB database wrapper class.
    Typical creation:
    >>> ejdb = pyejdb.EJDB("<db file path>", <db open mode>)

    :Parameters:
        - `fpath`: Database file path
        - `mode` (optional): Open mode int mask
            JBOREADER - Open as reader
            JBOWRITER - Open as writer
            JBOCREAT - Create new database if db file not exists
            JBOTRUNC - Truncate database on open
            JBONOLCK - Open without locking
            JBOLCKNB - Lock without blocking
            JBOTSYNC - Synchronize db on every transaction

    Default open mode: DEFAULT_OPEN_MODE = JBOWRITER | JBOCREAT | JBOTSYNC
    """

    def __init__(self, fpath, mode=DEFAULT_OPEN_MODE):
        self.__ejdb = _pyejdb.EJDB()
        self.__ejdb.open(fpath, mode)

    @property
    def isopen(self):
        return self.__ejdb.isopen()

    def close(self):
        return self.__ejdb.close()

    def sync(self):
        return self.__ejdb.sync()

    def save(self, cname, *jsarr, **kwargs):
        """ Save/update specified `dict` documents into collection `cname`.

        Samples:
        >>> ejdb.save('foo', {'foo' : 'bar'})
        >>> ejdb.save('foo', {_id : '511c72ae7922641d00000000', 'foo' : 'bar'}, {'foo' : 'bar2'}, ..., merge=True)
        >>> ejdb.save('foo', *[{'foo' : 'bar'}, {'foo' : 'bar2'}, ...])

        If collection with `cname` does not exists it will be created.
        Each document may have unique identifier (OID) stored in the `_id` property.
        If a saved doc does not have `_id` it will be autogenerated,
            document `dict` object will be updated with this `_id`
            and new document record will be stored.
        To identify and update doc it should contains `_id` dict property.

        :Parameters:
            - `cname` Collection name
            - `*jsarr` Variable arg list with doc to be saved
            -  merge=False (optional) If `True` docs with `_id` will be merged with those stored in db,
               otherwise updated documents will be fully replaced by new instances.
        """
        for doc in jsarr:
            if doc is not None:
                _oid = self.__ejdb.save(cname, bson.serialize_to_bytes(doc), **kwargs)
                if "_id" not in doc:
                    doc["_id"] = _oid

    def load(self, cname, oid):
        """ Loads `dict` documents identified by `oid` from `cname` collection.
        Sample:
        >>> ejdb.load('mycoll', '511c72ae7922641d00000000');
        :Parameters:
            - `cname` Collection name
            - `oid` Document object id (`_id` property)
        :Returns:
            A document `dict` identified by `oid` or `None` if it is not found
        """
        check_oid(oid)
        docbytes = self.__ejdb.load(cname, oid)
        if docbytes is None:
            return None
        return bson.parse_bytes(docbytes)

    def remove(self, cname, oid):
        """ Removes from `cname` collection the document identified by `oid`
        """
        check_oid(oid)
        return self.__ejdb.remove(cname, oid)

    def find(self, cname, qobj=None, *args, **kwargs):
        """ Execute query on collection.

        Sample:
        >>> # Fetch all elements from collection
        >>> ejdb.find("mycoll")
        >>> # Query document with 'foo==bar' condition, include in resulting doc 'foo' and '_id' fields only
        >>> ejdb.find("mycoll", {'foo' : 'bar'}, hints={$fields : {'foo' : 1, '_id' : 1}});

        General format:
            find(<collection name>, <query object>, <OR joined query objects>,..., hints=<Query Hints>)

        :Parameters:
           - `cname` Collection name
           - `qobj` Main query object
           - *args OR joined query object
           - hints={} (optional) Query hints. See explanations below.
           - log=StringIO (optional) StringIO buffer for ejdb query log (debugging mode)

        :Returns:
            Resultset cursor :class:`EJDBCursorWrapper`

        EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.
        - Supported queries:
           - Simple matching of String OR Number OR Array value:
               -   {'fpath' : 'val', ...}
           - $not Negate operation.
               -   {'fpath' : {'$not' : val}} //Field not equal to val
               -   {'fpath' : {'$not' : {'$begin' : prefix}}} //Field not begins with val
           - $begin String starts with prefix
               -   {'fpath' : {'$begin' : prefix}}
           - $gt, $gte (>, >=) and $lt, $lte for number types:
               -   {'fpath' : {'$gt' : number}, ...}
           - $bt Between for number types:
               -   {'fpath' : {'$bt' : [num1, num2]}}
           - $in String OR Number OR Array val matches to value in specified array:
               -   {'fpath' : {'$in' : [val1, val2, val3]}}
           - $nin - Not IN
           - $strand String tokens OR String array val matches all tokens in specified array:
               -   {'fpath' : {'$strand' : [val1, val2, val3]}}
           - $stror String tokens OR String array val matches any token in specified array:
               -   {'fpath' : {'$stror' : [val1, val2, val3]}}
           - $exists Field existence matching:
               -   {'fpath' : {'$exists' : true|false}}
           - $icase Case insensitive string matching:
               -    {'fpath' : {'$icase' : 'val1'}} //icase matching
               icase matching with '$in' operation:
               -    {'name' : {'$icase' : {'$in' : ['tHéâtre - театр', 'heLLo WorlD']}}}
               For case insensitive matching you can create special type of string index.
           - $elemMatch The $elemMatch operator matches more than one component within an array element.
               -    { array: { $elemMatch: { value1 : 1, value2 : { $gt: 1 } } } }
               Restriction: only one $elemMatch allowed in context of one array field.

        - Queries can be used to update records:

           - $set Field set operation.
               - {.., '$set' : {'field1' : val1, 'fieldN' : valN}}
           - $upsert Atomic upsert. If matching records are found it will be '$set' operation,
                     otherwise new record will be inserted with fields specified by argment object.
               - {.., '$upsert' : {'field1' : val1, 'fieldN' : valN}}
           - $inc Increment operation. Only number types are supported.
               - {.., '$inc' : {'field1' : number, ...,  'field1' : number}
           - $dropall In-place record removal operation.
               - {.., '$dropall' : true}
           - $addToSet Atomically adds value to the array only if its not in the array already.
                       If containing array is missing it will be created.
               - {.., '$addToSet' : {'fpath' : val1, 'fpathN' : valN, ...}}
           - $addToSetAll Batch version if $addToSet
               - {.., '$addToSetAll' : {'fpath' : [array of values to add], ...}}
           - $pull Atomically removes all occurrences of value from field, if field is an array.
               - {.., '$pull' : {'fpath' : val1, 'fpathN' : valN, ...}}
           - $pullAll Batch version of $pull
               - {.., '$pullAll' : {'fpath' : [array of values to remove], ...}}

        - Collection joins supported in the following form:
           - {..., $do : {fpath : {$join : 'collectionname'}} }
            Where 'fpath' value points to object's OIDs from 'collectionname'. Its value
            can be OID, string representation of OID or array of this pointers.

        .. NOTE:: It is better to execute update queries with `$onlycount=true` hint flag
               or use the special `update()` method to avoid unnecessarily data fetching.

        .. NOTE:: Negate operations: $not and $nin not using indexes
                 so they can be slow in comparison to other matching operations.

        .. NOTE:: Only one index can be used in search query operation.


        QUERY HINTS (specified by `hints=` argument):
           - $max Maximum number in the result set
           - $skip Number of skipped results in the result set
           - $orderby Sorting order of query fields.
           - $onlycount true|false If `true` only count of matching records will be returned
                                   without placing records in result set.
           - $fields Set subset of fetched fields.
               If field presented in $orderby clause it will be forced to include in resulting records.
               Example:
               hints:    {
                   "$orderby" : [ //ORDER BY field1 ASC, field2 DESC
                       ("field1", 1),
                       ("field2", -1)
                   ],
                   "$fields" : { //SELECT ONLY {_id, field1, field2}
                       "field1" : 1,
                       "field2" : 1
                   }
               }

        """
        if not qobj: qobj = {}
        qobj = bson.serialize_to_bytes(qobj)
        hints = bson.serialize_to_bytes(self.__preprocessQHints(kwargs.get("hints", {})))
        orarr = [bson.serialize_to_bytes(x) for x in args]
        qflags = kwargs.get("qflags", 0)
        log = kwargs.get("log")
        log = log if isinstance(log, StringIO) or isinstance(log, BytesIO) else None
        cursor = self.__ejdb.find(cname, qobj, orarr, hints, qflags, log)
        return cursor if isinstance(cursor, numbers.Number) else EJDBCursorWrapper(cursor)

    def findOne(self, cname, qobj=None, *args, **kwargs):
        """ Same as `#find()` but retrieves only one matching JSON object.
        """
        hints = self.__preprocessQHints(kwargs.get("hints", {}))
        hints["$max"] = 1
        kwargs["hints"] = hints
        with self.find(cname, qobj, *args, **kwargs) as res:
            return res[0] if len(res) > 0 else None

    def update(self, cname, qobj=None, *args, **kwargs):
        """ Convenient method to execute update queries.
        :Returns:
            Count of updated objects
        """
        qflags = kwargs.get("qflags", 0)
        kwargs["qflags"] = qflags | JBQRYCOUNT
        return self.find(cname, qobj, *args, **kwargs)

    def count(self, cname, qobj=None, *args, **kwargs):
        """ Counts matched documents.
        :Returns:
            Count of matched objects
        """
        qflags = kwargs.get("qflags", 0)
        kwargs["qflags"] = qflags | JBQRYCOUNT
        return self.find(cname, qobj, *args, **kwargs)

    def dbmeta(self):
        """ Retrieve metainfo object describing database structure.
        """
        return self.__ejdb.dbmeta()

    def begintx(self, cname):
        """ Begin collection transaction.
        """
        return self.__ejdb.txcontrol(cname, _pyejdb.PYEJDBTXBEGIN)

    def commitx(self, cname):
        """ Commit collection transaction.
        """
        return self.__ejdb.txcontrol(cname, _pyejdb.PYEJDBTXCOMMIT)

    def abortx(self, cname):
        """ Abort collection transaction.
        """
        return self.__ejdb.txcontrol(cname, _pyejdb.PYEJDBTXABORT)

    def isactivetx(self, cname):
        """ Is collection transaction active
        """
        return self.__ejdb.txcontrol(cname, _pyejdb.PYEJDBTXSTATUS)

    def ensureCollection(self, cname, **kwargs):
        """ Automatically creates new collection if it does not exists.
        Collection options `copts`
        are applied only for newly created collection.
        for existing collections `copts` takes no effect.
        Collection options (kwargs):
            {
               "cachedrecords" : Max number of cached records in shared memory segment. Default: 0
               "records" : Estimated number of records in this collection. Default: 65535.
               "large" : Specifies that the size of the database can be larger than 2GB. Default: false
               "compressed" : If true collection records will be compressed with DEFLATE compression. Default: false.
            }
        """
        return self.__ejdb.ensureCollection(cname, **kwargs)

    def dropCollection(self, cname, **kwargs):
        """ Removes database collection.
        """
        return self.__ejdb.dropCollection(cname, **kwargs)

    def dropIndexes(self, cname, path):
        """ Drops indexes of all types for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXDROPALL)

    def optimizeIndexes(self, cname, path):
        """ Optimize indexes of all types for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXOP)

    def ensureStringIndex(self, cname, path):
        """ Ensure index presence of String type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXSTR)

    def rebuildStringIndex(self, cname, path):
        """ Rebuild index of String type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXSTR | _pyejdb.JBIDXREBLD)

    def dropStringIndex(self, cname, path):
        """ Drop index of String type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXSTR | _pyejdb.JBIDXDROP)

    def ensureIStringIndex(self, cname, path):
        """ Ensure case insensitive String index for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXISTR)

    def rebuildIStringIndex(self, cname, path):
        """Rebuild case insensitive String index for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXISTR | _pyejdb.JBIDXREBLD)

    def dropIStringIndex(self, cname, path):
        """Drop case insensitive String index for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXISTR | _pyejdb.JBIDXDROP)

    def ensureNumberIndex(self, cname, path):
        """Ensure index presence of Number type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXNUM)

    def rebuildNumberIndex(self, cname, path):
        """Rebuild index of Number type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXNUM | _pyejdb.JBIDXREBLD)

    def dropNumberIndex(self, cname, path):
        """Drop index of Number type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXNUM | _pyejdb.JBIDXDROP)

    def ensureArrayIndex(self, cname, path):
        """Ensure index presence of Array type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXARR)

    def rebuildArrayIndex(self, cname, path):
        """Rebuild index of Array type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXARR | _pyejdb.JBIDXREBLD)

    def dropArrayIndex(self, cname, path):
        """Drop index of Array type for JSON field path.
        """
        return self.__ejdb.setIndex(cname, path, _pyejdb.JBIDXARR | _pyejdb.JBIDXDROP)

    def __preprocessQHints(self, hints):
        val = hints.get("$orderby")
        if isinstance(val, list):
            hints["$orderby"] = odict(val)
        return hints
