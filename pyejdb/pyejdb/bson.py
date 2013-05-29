#!/usr/bin/env python
#-*- coding: utf8 -*-
###############################################################################
#
# This module implements BSON parsing and serialization in accordance
# to version 1.0 of the specification, http://bsonspec.org/#/specification
#
# Usage:
#
# import bson
#
# b = bson.serialize_to_bytes({ "key": "value" }) # returns bytes
# d = bson.parse_bytes(b)                         # returns ordered dict
#
# bson.serialize_to_stream({ "key": "value" }, s) # writes to the stream
# d = bson.parse_stream(s)                        # returns ordered dict
#
# Notes:
#
# Certain BSON types have their natural counterparts in Python types,
# which are transparently converted back and forth at serialization
# and parsing. Specifically,
#
# int      <-> BSON_Int32 (or BSON_Int64 depending on the value)
# str      <-> BSON_String
# bytes    <-> BSON_Binary_Generic
# float    <-> BSON_Double
# bool     <-> BSON_Boolean
# datetime <-> BSON_Datetime
# None     <-> BSON_Null
# dict     <-> BSON_Document
# list     <-> BSON_Array
#
# Therefore you can serialize native Python types:
# b = bson.serialize_to_bytes({ "s": [ "foo", 123 ] })
# and parsing result is also in native types:
# assert bson.parse_bytes(b) == { "s": [ "foo", 123 ] }
#
# But some other BSON types have no such natural conversions,
# so you have to both create the objects manually:
# b = bson.serialize_to_bytes({ "r": bson.BSON_Regex(("^$", "i")) })
# and after parsing examine their "value" property:
# d = bson.parse_bytes(b); r = d["r"]
# assert isinstance(r, bson.BSON_Regex) and r.value == ("^$", "i")
#
# Additional notes:
#
# This module makes use of two utility modules typecheck.py and
# expected.py (both from Pythomnic3k framework) if they are present.
# You can download them from http://www.pythomnic3k.org/ or you
# can simply ignore the warnings if those modules are not found.
#
# Pythomnic3k project
# (c) 2005-2012, Dmitry Dvoinikov <dmitry@targeted.org>
# (c) 2012-2013 Softmotions Ltd <info@softmotions.com>
# Distributed under BSD license
#
###############################################################################

from __future__ import with_statement
from __future__ import division
from __future__ import print_function

__all__ = [

    # public methods

    "parse_bytes", "parse_stream", "serialize_to_bytes", "serialize_to_stream",

    # class hierarchy

    "BSON_Value",
    "BSON_Double",
    "BSON_String",
    "BSON_Document",
    "BSON_Array",
    "BSON_Binary",
    "BSON_Binary_Generic",
    "BSON_Binary_Function",
    "BSON_Binary_UUID",
    "BSON_Binary_MD5",
    "BSON_Binary_UserDefined",
    "BSON_ObjectId",
    "BSON_Boolean",
    "BSON_Datetime",
    "BSON_Null",
    "BSON_Regex",
    "BSON_JavaScript",
    "BSON_Symbol",
    "BSON_JavaScriptWithScope",
    "BSON_Int32",
    "BSON_Timestamp",
    "BSON_Int64",

    # errors hierarchy

    "BSON_Error",
    "BSON_AbstractError",
    "BSON_ParsingError",
    "BSON_ConversionError",

    # utilities

    "cstrify",

    ]

###############################################################################

import sys

if sys.version > "3":
    long = int
from datetime import datetime
from io import BytesIO
from struct import pack, unpack
from calendar import timegm
from collections import OrderedDict as odict
from binascii import hexlify, unhexlify

PY3 = sys.version_info[0] == 3

###############################################################################

cstrify = lambda s: s.encode("utf-8") + b"\x00"

###############################################################################

class BSON_Error(Exception): pass


class BSON_AbstractError(BSON_Error): pass


class BSON_ParsingError(BSON_Error): pass


class BSON_ConversionError(BSON_Error): pass


###############################################################################

class BSON_Value:
    def __init__(self, *args, **kwargs):
        raise BSON_AbstractError("cannot instantiate abstract base class")

    # assuming the real "value" of the object is in self._value

    value = property(lambda self: self._value)

    # subclasses override this for bs->py value conversion

    def _py_value(self):
        return self

    def __eq__(self, other):
        return self.__class__ is other.__class__ and\
               self._value == other._value

    def __ne__(self, other):
        return not self.__eq__(other)

    def __str__(self):
        return "{0:s}({1})".format(self.__class__.__name__, self._value)

    def __repr__(self):
        return "<{0:s} at 0x{1:08x}>".format(self, id(self))

    # subclasses are registered with their codes and conversion rules

    _types = {}   # maps byte codes to BSON_Type's for parsing

    @classmethod
    def _register_type(cls, type):
        cls._types[type._code] = type

    # redirects parsing to an appropriate BSON_Type by code

    @classmethod
    def parse(cls, code, stream):
        type = cls._types.get(code)
        if not type:
            raise BSON_ParsingError("unknown type code 0x{0:02x} at offset {1:d}".\
            format(int(code[0]), stream.tell())) # tested
        return type.parse(stream)

    # utility methods for stream reading

    @staticmethod
    def _read(stream, n):
        r = stream.read(n)
        if len(r) != n:
            raise BSON_ParsingError("unexpected end of stream") # tested
        return r

    @staticmethod
    def _read_null(stream):
        r = b""
        while True:
            b = BSON_Value._read(stream, 1)
            if b == b"\x00":
                return r
            r += b

###############################################################################

class BSON_Double(BSON_Value):
    _code = b"\x01"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        stream.write(pack("<d", self._value))

    @classmethod
    def parse(cls, stream):
        return cls(unpack("<d", cls._read(stream, 8))[0])

BSON_Value._register_type(BSON_Double)

###############################################################################

class BSON_String(BSON_Value):
    _code = b"\x02"

    def __init__(self, value):
        self._value = value

    def __str__(self):
        return "{0:s}('{1:s}')".format(self.__class__.__name__, self._value)

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        value_b = self._value.encode("utf-8")
        stream.write(pack("<l", len(value_b) + 1))
        stream.write(value_b)
        stream.write(b"\x00")

    @classmethod
    def parse(cls, stream):
        length = unpack("<l", cls._read(stream, 4))[0]
        if length <= 0:
            raise BSON_ParsingError("incorrect string length at offset {0:d}".\
            format(stream.tell())) # tested
        value_b = cls._read(stream, length)
        if value_b[-1] if PY3 else ord(value_b[-1]):
            raise BSON_ParsingError("expected null terminated string "
                                    "at offset {0:d}".format(stream.tell()))
        return cls(value_b[:-1].decode("utf-8")) # tested

BSON_Value._register_type(BSON_String)

###############################################################################

class BSON_LazyDict():
    def __init__(self, buf):
        self._buf = buf
        self._vdict = None

    def _check_lazy(self):
        if self._vdict is None:
            self._vdict = BSON_Document.parse(self._buf)._py_value()
            self._buf.close()
            self._buf = None
        return self._vdict

    def __setitem__(self, key, value):
        return self._check_lazy().__setitem__(key, value)

    def __getitem__(self, key):
        return self._check_lazy().__getitem__(key)

    def __repr__(self):
        return '%s' % type(self).__name__

    def update(self, *args, **kwargs):
        return self._check_lazy().update(*args, **kwargs)

    def __len__(self):
        return self._check_lazy().__len__()

    def __iter__(self):
        return self._check_lazy().__iter__()

    def __sizeof__(self):
        return self._check_lazy().__sizeof__()

    def __ge__(self, y):
        return self._check_lazy().__ge__(y)

    def __gt__(self, y):
        return self._check_lazy().__gt__(y)

    def __eq__(self, y):
        return self._check_lazy().__eq__(y)

    def __ne__(self, y):
        return self._check_lazy().__ne__(y)

    def __le__(self, y):
        return self._check_lazy().__le__(y)

    def __lt__(self, y):
        return self._check_lazy().__lt__(y)

    def __delitem__(self, y):
        return self._check_lazy().__delitem__(y)

    def __contains__(self, k):
        return self._check_lazy().__contains__(k)

    def setdefault(self, k, d=None):
        return self._check_lazy().setdefault(k, d)

    def items(self):
        return self._check_lazy().items()

    def popitem(self):
        return self._check_lazy().popitem()

    def keys(self):
        return self._check_lazy().keys()

    def get(self, k, d=None):
        return self._check_lazy().get(k, d)

    def copy(self):
        return self._check_lazy().copy()

    def values(self):
        return self._check_lazy().values()

    def clear(self):
        return self._check_lazy().clear()

    def pop(self, k, d=None):
        return self._check_lazy().pop(k, d)


class BSON_Document(BSON_Value):
    _code = b"\x03"

    def __init__(self, value):
        self._value = value
        #fix reserved _id field
        if "_id" in self._value and isinstance(self._value["_id"], BSON_String):
            self._value["_id"] = BSON_ObjectId(self._value["_id"]._py_value())

    def _py_value(self):
        return odict((k, v._py_value()) for k, v in self._value.items())

    def serialize(self, stream):
        e_list = BytesIO()
        for k, v in self._value.items():
            e_list.write(v._code)
            e_list.write(cstrify(k))
            v.serialize(e_list)
        e_list_b = e_list.getvalue()
        stream.write(pack("<l", len(e_list_b) + 5))
        stream.write(e_list_b)
        stream.write(b"\x00")

    @classmethod
    def parse(cls, stream):
        length = unpack("<l", cls._read(stream, 4))[0]
        if length <= 4:
            raise BSON_ParsingError("incorrect structure length at offset {0:d}".\
            format(stream.tell())) # tested
        epos = stream.tell() + length - 5
        doc = odict()
        while stream.tell() < epos:
            code_b = cls._read(stream, 1)
            key_b = cls._read_null(stream)
            doc[key_b.decode("utf-8")] = BSON_Value.parse(code_b, stream)
        if stream.tell() != epos:
            raise BSON_ParsingError("read beyond end of structure "
                                    "at offset {0:d}".format(stream.tell())) # tested
        if cls._read(stream, 1) != b"\x00":
            raise BSON_ParsingError("expected null terminated structure "
                                    "at offset {0:d}".format(stream.tell())) # tested
        return cls(doc)

BSON_Value._register_type(BSON_Document)

###############################################################################

class BSON_Array(BSON_Value):
    _code = b"\x04"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return [v._py_value() for v in self._value]

    def serialize(self, stream):
        d = odict((str(i), v) for i, v in enumerate(self._value))
        BSON_Document(d).serialize(stream)

    @classmethod
    def parse(cls, stream):
        d = BSON_Document.parse(stream).value
        return cls([d[str(i)] for i in range(len(d))])

BSON_Value._register_type(BSON_Array)

###############################################################################

class BSON_Binary(BSON_Value):
    _code = b"\x05"

    _subtypes = {}

    def __init__(self, *args, **kwargs):
        raise BSON_AbstractError("cannot instantiate abstract binary class")

    @classmethod
    def _register_subtype(cls, subtype, subcode=None):
        cls._subtypes[subtype._subcode if subcode is None else subcode] = subtype

    def serialize(self, stream):
        stream.write(pack("<l", len(self._value)))
        stream.write(self.__class__._subcode)
        stream.write(self._value)

    @classmethod
    def parse(cls, stream):
        length = unpack("<l", cls._read(stream, 4))[0]
        if length <= 0:
            raise BSON_ParsingError("incorrect structure length at offset {0:d}".\
            format(stream.tell())) # tested
        code = cls._read(stream, 1)
        content_b = cls._read(stream, length)
        return cls._subtypes[code](content_b) # treated as opaque bytes

BSON_Value._register_type(BSON_Binary)

###############################################################################

class BSON_Binary_Generic(BSON_Binary):
    _subcode = b"\00"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value if PY3 else self


BSON_Binary._register_subtype(BSON_Binary_Generic)
BSON_Binary._register_subtype(BSON_Binary_Generic, b"\x02") # deprecated alternative

###############################################################################

class BSON_Binary_Function(BSON_Binary):
    _subcode = b"\01"

    def __init__(self, value):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_Function)

###############################################################################

class BSON_Binary_UUID(BSON_Binary):
    _subcode = b"\03"

    def __init__(self, value):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_UUID)

###############################################################################

class BSON_Binary_MD5(BSON_Binary):
    _subcode = b"\05"

    def __init__(self, value):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_MD5)

###############################################################################

class BSON_Binary_UserDefined(BSON_Binary):
    _subcode = b"\x80"

    def __init__(self, value):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_UserDefined)

###############################################################################

class BSON_ObjectId(BSON_Value):
    _code = b"\x07"

    def __init__(self, value):
        if PY3:
            if not (isinstance(value, bytes) and len(value) == 12) and not (isinstance(value, str) and len(value) == 24):
                raise BSON_ConversionError("Invalid representation of object ID")
        else:
            tn = type(value).__name__
            if not (len(value) == 12 and tn == "str") and not (len(value) == 24 and (tn == "str" or tn == "unicode")):
                raise BSON_ConversionError("Invalid representation of object ID")

        if len(value) == 12:
            self._value = value
        else:
            self._value = unhexlify(value.encode("ascii"))


    def __str__(self):
        return "{0:s}(0x{1:s})".format(self.__class__.__name__,
                                       hexlify(self._value).decode("ascii"))

    def _py_value(self):
        return hexlify(self._value).decode("ascii")

    def serialize(self, stream):
        stream.write(self._value)

    @classmethod
    def parse(cls, stream):
        return cls(cls._read(stream, 12))

BSON_Value._register_type(BSON_ObjectId)

###############################################################################

class BSON_Boolean(BSON_Value):
    _code = b"\x08"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        stream.write(self._value and b"\x01" or b"\x00")

    @classmethod
    def parse(cls, stream):
        b = cls._read(stream, 1)[0]
        if not PY3:
            b = ord(b)
        if b not in (0, 1):
            raise BSON_ParsingError("incorrect boolean value at offset {0:d}".\
            format(stream.tell())) # tested
        return cls(b == 1)

BSON_Value._register_type(BSON_Boolean)

###############################################################################

class BSON_Datetime(BSON_Value):
    _code = b"\x09"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        epoch_ms = long(timegm(self._value.utctimetuple()) * 1000 + self._value.microsecond // 1000)
        BSON_Int64(epoch_ms).serialize(stream)

    @classmethod
    def parse(cls, stream):
        epoch_ms = BSON_Int64.parse(stream).value
        return cls(datetime.utcfromtimestamp(epoch_ms / 1000.0))

BSON_Value._register_type(BSON_Datetime)

###############################################################################

class BSON_Null(BSON_Value):
    _code = b"\x0a"

    def __init__(self, value):
        self._value = value

    def __str__(self):
        return "{0:s}()".format(self.__class__.__name__)

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        pass

    @classmethod
    def parse(cls, stream):
        return cls(None)

BSON_Value._register_type(BSON_Null)

###############################################################################

class BSON_Regex(BSON_Value):
    _code = b"\x0b"

    def __init__(self, value, *args, **kwargs):
        self._value = value

    def __str__(self):
        return "{0:s}('{1[0]:s}', '{1[1]:s}')".\
        format(self.__class__.__name__, self._value)

    def serialize(self, stream):
        stream.write(cstrify(self._value[0]))
        stream.write(cstrify(self._value[1]))

    @classmethod
    def parse(cls, stream):
        pattern_b = cls._read_null(stream)
        options_b = cls._read_null(stream)
        return cls((pattern_b.decode("utf-8"), options_b.decode("utf-8")))

BSON_Value._register_type(BSON_Regex)

###############################################################################

class BSON_JavaScript(BSON_Value):
    _code = b"\x0d"

    def __init__(self, value):
        self._value = value

    def __str__(self):
        return "{0:s}('{1:s}')".format(self.__class__.__name__, self._value)

    def serialize(self, stream):
        BSON_String(self._value).serialize(stream)

    @classmethod
    def parse(cls, stream):
        return cls(BSON_String.parse(stream).value)

BSON_Value._register_type(BSON_JavaScript)

###############################################################################

class BSON_Symbol(BSON_Value):
    _code = b"\x0e"

    def __init__(self, value):
        self._value = value

    def serialize(self, stream):
        BSON_String(self._value).serialize(stream)

    @classmethod
    def parse(cls, stream):
        return cls(BSON_String.parse(stream).value)

BSON_Value._register_type(BSON_Symbol)

###############################################################################

class BSON_JavaScriptWithScope(BSON_Value):
    _code = b"\x0f"

    def __init__(self, value):
        self._value = value

    def __str__(self):
        return "{0:s}('{1[0]:s}', {1[1]:s})".\
        format(self.__class__.__name__, self._value)

    def serialize(self, stream):
        codews = BytesIO()
        BSON_String(self._value[0]).serialize(codews)
        self._value[1].serialize(codews)
        codews_b = codews.getvalue()
        stream.write(pack("<l", len(codews_b) + 4))
        stream.write(codews_b)

    @classmethod
    def parse(cls, stream):
        length = unpack("<l", cls._read(stream, 4))[0]
        if length <= 13:
            raise BSON_ParsingError("incorrect structure length at offset {0:d}".\
            format(stream.tell())) # tested
        epos = stream.tell() + length - 4
        code = BSON_String.parse(stream)
        scope = BSON_Document.parse(stream)
        if stream.tell() != epos:
            raise BSON_ParsingError("read beyond end of structure at offset {0:d}".\
            format(stream.tell())) # tested
        return cls((code.value, scope))

BSON_Value._register_type(BSON_JavaScriptWithScope)

###############################################################################

class BSON_Int32(BSON_Value):
    _code = b"\x10"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        stream.write(pack("<l", self._value))

    @classmethod
    def parse(cls, stream):
        return cls(unpack("<l", cls._read(stream, 4))[0])

BSON_Value._register_type(BSON_Int32)

###############################################################################

class BSON_Timestamp(BSON_Value):
    _code = b"\x11"

    def __init__(self, value):
        self._value = value

    def serialize(self, stream):
        BSON_Int64(self._value).serialize(stream)

    @classmethod
    def parse(cls, stream):
        return cls(BSON_Int64.parse(stream).value)

BSON_Value._register_type(BSON_Timestamp)

###############################################################################

class BSON_Int64(BSON_Value):
    _code = b"\x12"

    def __init__(self, value):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        stream.write(pack("<q", self._value))

    @classmethod
    def parse(cls, stream):
        return cls(unpack("<q", cls._read(stream, 8))[0])

BSON_Value._register_type(BSON_Int64)

###############################################################################

def _py_no_bs(v):
    raise BSON_ConversionError("cannot implicitly convert {0:s} value {1}".\
    format(v.__class__.__name__, v)) # tested

if PY3:
    _py_to_bs =\
        {
        float: lambda f: BSON_Double(f),
        str: lambda s: BSON_String(s),
        dict: lambda d: BSON_Document(odict((str(k), py_to_bs(v)) for k, v in d.items())),
        BSON_LazyDict: lambda d: BSON_Document(d._check_lazy()),
        list: lambda l: BSON_Array([py_to_bs(v) for v in l]),
        int: lambda i: BSON_Int32(i) if -2 ** 31 <= i <= 2 ** 31 - 1 else BSON_Int64(
            i) if -2 ** 63 <= i <= 2 ** 63 - 1 else _py_no_bs(i),
        bytes: lambda b: BSON_Binary_Generic(b),
        BytesIO: lambda b: BSON_Binary_Generic(b.getvalue()),
        bool: lambda b: BSON_Boolean(b),
        datetime: lambda dt: BSON_Datetime(dt),
        type(None): lambda n: BSON_Null(n),
        }
else:
    _py_to_bs =\
        {
        float: lambda f: BSON_Double(f),
        str: lambda s: BSON_String(s),
        unicode: lambda s: BSON_String(s),
        dict: lambda d: BSON_Document(odict((str(k), py_to_bs(v)) for k, v in d.items())),
        BSON_LazyDict: lambda d: BSON_Document(d._check_lazy()),
        list: lambda l: BSON_Array([py_to_bs(v) for v in l]),
        int: lambda i: BSON_Int32(i) if -2 ** 31 <= i <= 2 ** 31 - 1 else BSON_Int64(
            i) if -2 ** 63 <= i <= 2 ** 63 - 1 else _py_no_bs(i),
        long: lambda i: BSON_Int32(i) if -2 ** 31 <= i <= 2 ** 31 - 1 else BSON_Int64(
            i) if -2 ** 63 <= i <= 2 ** 63 - 1 else _py_no_bs(i),
        BytesIO: lambda b: BSON_Binary_Generic(b.getvalue()),
        bool: lambda b: BSON_Boolean(b),
        datetime: lambda dt: BSON_Datetime(dt),
        type(None): lambda n: BSON_Null(n),
        }

def py_to_bs(v):
    if isinstance(v, BSON_Value):
        return v
    f = _py_to_bs.get(v.__class__)
    if f: return f(v)
    for t, f in _py_to_bs.items():
        if isinstance(v, t):
            return f(v)
    _py_no_bs(v)


def bs_to_py(v):
    return v._py_value()

###############################################################################

def serialize_to_bytes(document):
    stream = BytesIO()
    serialize_to_stream(document, stream)
    return stream.getvalue()


def serialize_to_stream(document, stream):
    py_to_bs(document).serialize(stream)
    stream.flush()

###############################################################################

def parse_bytes(serialized):
    stream = BytesIO(serialized)
    return parse_stream(stream)

def parse_bytes_lazy(serialized):
    return BSON_LazyDict(BytesIO(serialized))


def parse_stream(stream):
    return bs_to_py(BSON_Document.parse(stream))

###############################################################################

#if __name__ == "__main__": # self-test
#
#    from random import shuffle
#
#    try:
#        import expected
#    except ImportError:
#        print("warning: module expected.py cannot be imported, exception tests are skipped")
#
#        class expected:
#            def __init__(self, *args, **kwargs):
#                pass
#
#            def __enter__(self):
#                pass
#
#            def __exit__(self, t, v, tb):
#                return True
#    else:
#        from expected import expected
#
#    ###################################
#
#    print("self-testing module bson.py:")
#
#    ###################################
#
#    print("utilities: ", end="")
#
#    assert cstrify("") == b"\x00"
#    assert cstrify("foo") == b"foo\x00"
#    assert cstrify(u"абв") == b"\xd0\xb0\xd0\xb1\xd0\xb2\x00"
#
#    print("ok")
#
#    ###################################
#
#    print("exact byte sequences: ", end="")
#
#    def test_type(t, v, b):
#        bio = BytesIO()
#        t(v).serialize(bio)
#        assert bio.getvalue() == b, bio.getvalue()
#        bio.seek(0, 0)
#        assert t.parse(bio).value == v
#        assert bio.tell() == len(bio.getvalue())
#
#    test_type(BSON_Double, 0.0, b"\x00\x00\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Double, 1.0, b"\x00\x00\x00\x00\x00\x00\xf0?")
#    test_type(BSON_Double, -1.0, b"\x00\x00\x00\x00\x00\x00\xf0\xbf")
#    test_type(BSON_Double, 1e308, b"\xa0\xc8\xeb\x85\xf3\xcc\xe1\x7f")
#    test_type(BSON_Double, 1e-308, b"\xd2\xe8\x19x\xd60\x07\x00")
#    test_type(BSON_Double, -1e308, b"\xa0\xc8\xeb\x85\xf3\xcc\xe1\xff")
#    test_type(BSON_Double, -1e-308, b"\xd2\xe8\x19x\xd60\x07\x80")
#
#    test_type(BSON_Int32, 0, b"\x00\x00\x00\x00")
#    test_type(BSON_Int32, 1, b"\x01\x00\x00\x00")
#    test_type(BSON_Int32, -1, b"\xff\xff\xff\xff")
#    test_type(BSON_Int32, 2 ** 31 - 1, b"\xff\xff\xff\x7f")
#    test_type(BSON_Int32, -2 ** 31, b"\x00\x00\x00\x80")
#
#    test_type(BSON_Int64, 0, b"\x00\x00\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Int64, 1, b"\x01\x00\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Int64, -1, b"\xff\xff\xff\xff\xff\xff\xff\xff")
#    test_type(BSON_Int64, 2 ** 63 - 1, b"\xff\xff\xff\xff\xff\xff\xff\x7f")
#    test_type(BSON_Int64, -2 ** 63, b"\x00\x00\x00\x00\x00\x00\x00\x80")
#
#    test_type(BSON_String, "", b"\x01\x00\x00\x00\x00")
#    test_type(BSON_String, "foo", b"\x04\x00\x00\x00foo\x00")
#    test_type(BSON_String, u"абв", b"\x07\x00\x00\x00\xd0\xb0\xd0\xb1\xd0\xb2\x00")
#
#    test_type(BSON_Boolean, True, b"\x01")
#    test_type(BSON_Boolean, False, b"\x00")
#
#    test_type(BSON_ObjectId, b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12",
#              b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12")
#
#    test_type(BSON_Null, None, b"")
#
#    test_type(BSON_JavaScript, "var i = 1;", b"\x0b\x00\x00\x00var i = 1;\x00")
#    test_type(BSON_JavaScript, u"/* комментарий */",
#              b"\x1d\x00\x00\x00/* \xd0\xba\xd0\xbe\xd0\xbc\xd0\xbc\xd0\xb5\xd0\xbd\xd1\x82\xd0\xb0\xd1\x80\xd0\xb8\xd0\xb9 */\x00")
#
#    test_type(BSON_Timestamp, 1, b"\x01\x00\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Timestamp, -1, b"\xff\xff\xff\xff\xff\xff\xff\xff")
#    test_type(BSON_Timestamp, 2 ** 63 - 1, b"\xff\xff\xff\xff\xff\xff\xff\x7f")
#    test_type(BSON_Timestamp, -2 ** 63, b"\x00\x00\x00\x00\x00\x00\x00\x80")
#
#    test_type(BSON_Datetime, datetime(1970, 1, 1, 0, 0, 0), b"\x00\x00\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Datetime, datetime(1970, 1, 1, 0, 0, 1), b"\xe8\x03\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_Datetime, datetime(2010, 12, 12, 15, 11, 43), b"\x98\x27\x23\xdb\x2c\x01\x00\x00")
#
#    test_type(BSON_Symbol, "foo", b"\x04\x00\x00\x00foo\x00")
#    test_type(BSON_Symbol, u"символ",
#              b"\x0d\x00\x00\x00\xd1\x81\xd0\xb8\xd0\xbc\xd0\xb2\xd0\xbe\xd0\xbb\x00")
#
#    test_type(BSON_JavaScriptWithScope, ("var i = j + 1;", BSON_Document({"j": BSON_Int32(0)})),
#              b"\x23\x00\x00\x00\x0f\x00\x00\x00var i = j + 1;\x00\x0c\x00\x00\x00\x10j\x00\x00\x00\x00\x00\x00")
#    test_type(BSON_JavaScriptWithScope, ("foo(s);", BSON_Document({"s": BSON_String(u"абв")})),
#              b"\x23\x00\x00\x00\x08\x00\x00\x00foo(s);\x00\x13\x00\x00\x00\x02s\x00\x07\x00\x00\x00\xd0\xb0\xd0\xb1\xd0\xb2\x00\x00")
#
#    test_type(BSON_Regex, ("", ""), b"\x00\x00")
#    test_type(BSON_Regex, ("^$", "i"), b"^$\x00i\x00")
#
#    test_type(BSON_Binary_Generic, b"value", b"\x05\x00\x00\x00\x00value")
#    test_type(BSON_Binary_Function, b"value", b"\x05\x00\x00\x00\x01value")
#    test_type(BSON_Binary_UUID, b"value", b"\x05\x00\x00\x00\x03value")
#    test_type(BSON_Binary_MD5, b"value", b"\x05\x00\x00\x00\x05value")
#    test_type(BSON_Binary_UserDefined, b"value", b"\x05\x00\x00\x00\x80value")
#
#    test_type(BSON_Document, {}, b"\x05\x00\x00\x00\x00")
#    test_type(BSON_Document, {"foo": BSON_Null(None)}, b"\x0a\x00\x00\x00\x0afoo\x00\x00")
#
#    print("ok")
#
#    ###################################
#
#    print("forward type mapping: ", end="")
#
#    assert py_to_bs(1.0) == BSON_Double(1.0)
#    assert py_to_bs("foo") == BSON_String("foo")
#    assert py_to_bs(True) == BSON_Boolean(True)
#    dt = datetime.now();
#    assert py_to_bs(dt) == BSON_Datetime(dt)
#    assert py_to_bs(None) == BSON_Null(None)
#    assert py_to_bs(2 ** 31 - 1) == BSON_Int32(2 ** 31 - 1)
#    assert py_to_bs(2 ** 31) == BSON_Int64(2 ** 31)
#
#    def py_to_bs_same(v):
#        return py_to_bs(v) is v
#
#    assert py_to_bs_same(BSON_ObjectId(b"\x00" * 12))
#    assert py_to_bs_same(BSON_Regex(("^$", "i")))
#    assert py_to_bs_same(BSON_JavaScript("var i = 1;"))
#    assert py_to_bs_same(BSON_Symbol("class"))
#    assert py_to_bs_same(BSON_JavaScriptWithScope(("var i = j;", BSON_Document({"j": BSON_Int32(10)}))))
#    assert py_to_bs_same(BSON_Timestamp(0x7fffffffffffffff))
#
#    assert py_to_bs([]) == BSON_Array([])
#    assert py_to_bs(
#        [
#            b"implicit" if PY3 else BytesIO("implicit"),
#            BSON_Binary_Generic(b"generic"),
#            BSON_Binary_Function(b"function"),
#            BSON_Binary_UUID(b"uuid"),
#            BSON_Binary_MD5(b"md5"),
#            BSON_Binary_UserDefined(b"userdefined"),
#            ]) ==\
#           BSON_Array(
#               [
#                   BSON_Binary_Generic(b"implicit"),
#                   BSON_Binary_Generic(b"generic"),
#                   BSON_Binary_Function(b"function"),
#                   BSON_Binary_UUID(b"uuid"),
#                   BSON_Binary_MD5(b"md5"),
#                   BSON_Binary_UserDefined(b"userdefined"),
#                   ])
#
#    assert py_to_bs({}) == BSON_Document({})
#    assert py_to_bs(
#            {
#            "array": [{}, {}],
#            "document": {"key": []},
#            }) ==\
#           BSON_Document(
#                   {
#                   "array": BSON_Array([BSON_Document({}), BSON_Document({})]),
#                   "document": BSON_Document({"key": BSON_Array([])}),
#                   })
#
#    with expected(BSON_ConversionError, "cannot implicitly convert int value 9223372036854775808"):
#        py_to_bs(2 ** 63)
#
#    with expected(BSON_ConversionError, "cannot implicitly convert tuple value \(\)"):
#        py_to_bs(())
#
#    print("ok")
#
#    ###################################
#
#    print("document key ordering: ", end="")
#
#    class Foo(int):
#        pass
#
#    ks1 = list(range(10000))
#    shuffle(ks1)
#    d1 = odict((str(k), Foo(k)) for k in ks1)
#    b = serialize_to_bytes(d1)
#    d2 = parse_bytes(b)
#    ks2 = list(d2.values())
#    assert ks2 == ks1
#
#    print("ok")
#
#    ###################################
#
#    print("reverse type mapping: ", end="")
#
#    assert bs_to_py(BSON_Double(1.0)) == 1.0
#    assert bs_to_py(BSON_String("foo")) == "foo"
#    assert bs_to_py(BSON_Boolean(True)) == True
#    dt = datetime.now()
#    assert bs_to_py(BSON_Datetime(dt)) == dt
#    assert bs_to_py(BSON_Null(None)) == None
#    assert bs_to_py(BSON_Int32(2 ** 31 - 1)) == 2 ** 31 - 1
#    assert bs_to_py(BSON_Int64(2 ** 31)) == 2 ** 31
#
#    def bs_to_py_same(v):
#        return bs_to_py(v) is v
#
#    assert type(BSON_ObjectId(b"\x00" * 12)._py_value()).__name__ == 'str' if PY3 else 'unicode'
#    assert bs_to_py_same(BSON_Regex(("^$", "")))
#    assert bs_to_py_same(BSON_JavaScript("var i = 0;"))
#    assert bs_to_py_same(BSON_Symbol("class"))
#    assert bs_to_py_same(BSON_JavaScriptWithScope(("var i = j;", BSON_Document({"j": -1}))))
#    assert bs_to_py_same(BSON_Timestamp(0x7fffffffffffffff))
#
#    assert bs_to_py(BSON_Array([])) == []
#
#    assert bs_to_py(BSON_Array(
#        [
#            BSON_Binary_Generic(b"generic"),
#            BSON_Binary_Function(b"function"),
#            BSON_Binary_UUID(b"uuid"),
#            BSON_Binary_MD5(b"md5"),
#            BSON_Binary_UserDefined(b"userdefined"),
#            ])) ==\
#           [
#               b"generic" if PY3 else BSON_Binary_Generic(b"generic"),
#               BSON_Binary_Function(b"function"),
#               BSON_Binary_UUID(b"uuid"),
#               BSON_Binary_MD5(b"md5"),
#               BSON_Binary_UserDefined(b"userdefined"),
#               ]
#
#    assert bs_to_py(BSON_Document({})) == {}
#    assert bs_to_py(BSON_Document(
#            {
#            "array": BSON_Array([BSON_Document({}), BSON_Document({})]),
#            "document": BSON_Document({"key": BSON_Array([])}),
#            })) ==\
#           {
#               "array": [{}, {}],
#               "document": {"key": []},
#               }
#
#    print("ok")
#
#    ###################################
#
#    print("deprecated quirks: ", end="")
#
#    bio = BytesIO(b"\x05\x00\x00\x00\x02value")
#    assert BSON_Binary.parse(bio) == BSON_Binary_Generic(b"value")
#
#    with expected(BSON_ParsingError, "unknown type code 0x06 at offset 0"):
#        print(BSON_Value.parse(b"\x06", BytesIO()))
#
#    with expected(BSON_ParsingError, "unknown type code 0x0c at offset 0"):
#        print(BSON_Value.parse(b"\x0c", BytesIO()))
#
#    print("ok")
#
#    ###################################
#
#    print("specification samples: ", end="")
#
#    assert serialize_to_bytes({"hello": "world"}) ==\
#           b"\x16\x00\x00\x00\x02hello\x00\x06\x00\x00\x00world\x00\x00"
#
#    assert parse_bytes(b"\x16\x00\x00\x00\x02hello\x00\x06\x00\x00\x00world\x00\x00") ==\
#           {"hello": "world"}
#
#    assert serialize_to_bytes({"BSON": ["awesome", 5.05, 1986]}) ==\
#           b"1\x00\x00\x00\x04BSON\x00&\x00\x00\x00\x020\x00\x08\x00\x00\x00"\
#           b"awesome\x00\x011\x00333333\x14@\x102\x00\xc2\x07\x00\x00\x00\x00"
#
#    assert parse_bytes(b"1\x00\x00\x00\x04BSON\x00&\x00\x00\x00\x020\x00\x08\x00\x00\x00"
#                       b"awesome\x00\x011\x00333333\x14@\x102\x00\xc2\x07\x00\x00\x00\x00") ==\
#           {"BSON": ["awesome", 5.05, 1986]}
#
#    print("ok")
#
#    ###################################
#
#    print("single type values: ", end="")
#
#    b = serialize_to_bytes({"double": 1.0})
#    assert b == serialize_to_bytes({"double": BSON_Double(1.0)})
#    assert parse_bytes(b) == {"double": 1.0}
#
#    b = serialize_to_bytes({"string": u"абв"})
#    assert b == serialize_to_bytes({"string": BSON_String(u"абв")})
#    assert parse_bytes(b) == {"string": u"абв"}
#
#    b = serialize_to_bytes({"boolean": True})
#    assert b == serialize_to_bytes({"boolean": BSON_Boolean(True)})
#    assert parse_bytes(b) == {"boolean": True}
#
#    dt = datetime.now()
#    b = serialize_to_bytes({"datetime": dt})
#    assert b == serialize_to_bytes({"datetime": BSON_Datetime(dt)})
#    r = parse_bytes(b)
#    dt2 = r.pop("datetime")
#    assert not r
#    dt = dt.replace(microsecond=0)
#    dt2 = dt2.replace(microsecond=0)
#    assert dt == dt2
#
#    b = serialize_to_bytes({"null": None})
#    assert b == serialize_to_bytes({"null": BSON_Null(None)})
#    assert parse_bytes(b) == {"null": None}
#
#    b = serialize_to_bytes({"int32": 2 ** 31 - 1})
#    assert b == serialize_to_bytes({"int32": BSON_Int32(2 ** 31 - 1)})
#    assert parse_bytes(b) == {"int32": 2 ** 31 - 1}
#
#    b = serialize_to_bytes({"int64": 2 ** 63 - 1})
#    assert b == serialize_to_bytes({"int64": BSON_Int64(2 ** 63 - 1)})
#    assert parse_bytes(b) == {"int64": 2 ** 63 - 1}
#
#    bin = bytes([i for i in range(256)])
#    b = serialize_to_bytes({"bytes": BytesIO(bin)})
#    assert b == serialize_to_bytes({"bytes": BSON_Binary_Generic(bin)})
#    assert parse_bytes(b) == {"bytes": bin if PY3 else BSON_Binary_Generic(bin)}
#
#    b = serialize_to_bytes({"function": BSON_Binary_Function(b"function")})
#    assert parse_bytes(b) == {"function": BSON_Binary_Function(b"function")}
#
#    b = serialize_to_bytes({"uuid": BSON_Binary_UUID(b"uuid")})
#    assert parse_bytes(b) == {"uuid": BSON_Binary_UUID(b"uuid")}
#
#    b = serialize_to_bytes({"md5": BSON_Binary_MD5(b"md5")})
#    assert parse_bytes(b) == {"md5": BSON_Binary_MD5(b"md5")}
#
#    b = serialize_to_bytes({"userdefined": BSON_Binary_UserDefined(b"userdefined")})
#    assert parse_bytes(b) == {"userdefined": BSON_Binary_UserDefined(b"userdefined")}
#
#    b = serialize_to_bytes({"objectid": BSON_ObjectId(b"\xff" * 12)})
#    assert parse_bytes(b) == {"objectid": BSON_ObjectId(b"\xff" * 12)._py_value()}
#
#    b = serialize_to_bytes({"regex": BSON_Regex(("^$", "i"))})
#    assert parse_bytes(b) == {"regex": BSON_Regex(("^$", "i"))}
#
#    b = serialize_to_bytes({"javascript": BSON_JavaScript("var i = 1;")})
#    assert parse_bytes(b) == {"javascript": BSON_JavaScript("var i = 1;")}
#
#    b = serialize_to_bytes({"symbol": BSON_Symbol("class")})
#    assert parse_bytes(b) == {"symbol": BSON_Symbol("class")}
#
#    b = serialize_to_bytes(
#            {"javascript_ws": BSON_JavaScriptWithScope(("var i = j;", BSON_Document({"i": BSON_Int32(1)})))})
#    assert parse_bytes(b) == {
#        "javascript_ws": BSON_JavaScriptWithScope(("var i = j;", BSON_Document({"i": BSON_Int32(1)})))}
#
#    b = serialize_to_bytes({"timestamp": BSON_Timestamp(0)})
#    assert parse_bytes(b) == {"timestamp": BSON_Timestamp(0)}
#
#    b = serialize_to_bytes({"document": {}})
#    assert parse_bytes(b) == {"document": {}}
#
#    b = serialize_to_bytes({"array": []})
#    assert parse_bytes(b) == {"array": []}
#
#    print("ok")
#
#    ###################################
#
#    print("parsing errors: ", end="")
#
#    with expected(BSON_ParsingError, "incorrect structure length at offset 4"):
#        parse_bytes(b"\x00\x00\x00\x00")
#
#    with expected(BSON_ParsingError, "unexpected end of stream"):
#        parse_bytes(b"\x05\x00\x00\x00")
#
#    with expected(BSON_ParsingError, "expected null terminated structure at offset 5"):
#        parse_bytes(b"\x05\x00\x00\x00\xff")
#
#    with expected(BSON_ParsingError, "unknown type code 0xff at offset 6"):
#        parse_bytes(b"\x06\x00\x00\x00\xff\x00")
#
#    with expected(BSON_ParsingError, "read beyond end of structure at offset 14"):
#        parse_bytes(b"\x0e\x00\x00\x00\x02\x00\x04\x00\x00\x00foo\x00")
#
#    with expected(BSON_ParsingError, "incorrect string length at offset 10"):
#        parse_bytes(b"\x0a\x00\x00\x00\x02\x00\x00\x00\x00\x00")
#
#    with expected(BSON_ParsingError, "expected null terminated string at offset 13"):
#        parse_bytes(b"\x0d\x00\x00\x00\x02\x00\x03\x00\x00\x00foo")
#
#    with expected(BSON_ParsingError, "incorrect structure length at offset 10"):
#        parse_bytes(b"\x06\x00\x00\x00\x05\x00\x00\x00\x00\x00")
#
#    with expected(BSON_ParsingError, "incorrect boolean value at offset 7"):
#        parse_bytes(b"\x07\x00\x00\x00\x08\x00\xff")
#
#    with expected(BSON_ParsingError, "incorrect structure length at offset 10"):
#        parse_bytes(b"\x0a\x00\x00\x00\x0f\x00\x04\x00\x00\x00")
#
#    with expected(BSON_ParsingError, "read beyond end of structure at offset 41"):
#        parse_bytes(b"\x2a\x00\x00\x00\x0fjsws\x00\x1e\x00\x00\x00\x0b\x00\x00\x00"
#                    b"var i = j;\x00\x0c\x00\x00\x00\x10j\x00\n\x00\x00\x00\x00\x00")
#
#    print("ok")
#
#    ###################################
#
#    print("all ok")

# EOF


