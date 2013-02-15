#!/usr/bin/env python3
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
# Distributed under BSD license
#
###############################################################################

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

import io; from io import BytesIO
import struct; from struct import pack, unpack
import datetime; from datetime import datetime
import calendar; from calendar import timegm
import collections; from collections import OrderedDict as odict
import binascii; from binascii import hexlify, unhexlify
try:
    import pyejdb.typecheck
except ImportError:
    print("warning: module typecheck.py cannot be imported, type checking is skipped")
    typecheck = lambda x: x; optional = callable = with_attr = lambda *args: True
else:
    from pyejdb.typecheck import typecheck, optional, callable, with_attr

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
        return self.__class__ is other.__class__ and \
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
    def _read(stream, n) -> bytes:
        r = stream.read(n)
        if len(r) != n:
            raise BSON_ParsingError("unexpected end of stream") # tested
        return r

    @staticmethod
    def _read_null(stream) -> bytes:
        r = b""
        while True:
            b = BSON_Value._read(stream, 1)
            if b == b"\x00":
                return r
            r += b

###############################################################################

class BSON_Double(BSON_Value):

    _code = b"\x01"

    @typecheck
    def __init__(self, value: float):
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

    @typecheck
    def __init__(self, value: str):
        self._value = value

    def __str__(self):
        return "{0:s}('{1:s}')".format(self.__class__.__name__, self._value)

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        value_b = self._value.encode("utf-8")
        stream.write(pack("<l", len(value_b) + 1))
        stream.write(value_b); stream.write(b"\x00")

    @classmethod
    def parse(cls, stream):
        length = unpack("<l", cls._read(stream, 4))[0]
        if length <= 0:
            raise BSON_ParsingError("incorrect string length at offset {0:d}".\
                                    format(stream.tell())) # tested
        value_b = cls._read(stream, length)
        if value_b[-1] != 0:
            raise BSON_ParsingError("expected null terminated string "
                                    "at offset {0:d}".format(stream.tell()))
        return cls(value_b[:-1].decode("utf-8")) # tested

BSON_Value._register_type(BSON_String)

###############################################################################

class BSON_Document(BSON_Value):

    _code = b"\x03"

    @typecheck
    def __init__(self, value: dict):
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
        stream.write(e_list_b); stream.write(b"\x00")

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

    @typecheck
    def __init__(self, value: list):
        self._value = value

    def _py_value(self):
        return [ v._py_value() for v in self._value ]

    def serialize(self, stream):
        d = odict((str(i), v) for i, v in enumerate(self._value))
        BSON_Document(d).serialize(stream)

    @classmethod
    def parse(cls, stream):
        d = BSON_Document.parse(stream).value
        return cls([ d[str(i)] for i in range(len(d)) ])

BSON_Value._register_type(BSON_Array)

###############################################################################

class BSON_Binary(BSON_Value):

    _code = b"\x05"

    _subtypes = {}

    def __init__(self, *args, **kwargs):
        raise BSON_AbstractError("cannot instantiate abstract binary class")

    @classmethod
    def _register_subtype(cls, subtype, subcode = None):
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

    @typecheck
    def __init__(self, value: bytes):
        self._value = value

    def _py_value(self):
        return self._value

BSON_Binary._register_subtype(BSON_Binary_Generic)
BSON_Binary._register_subtype(BSON_Binary_Generic, b"\x02") # deprecated alternative

###############################################################################

class BSON_Binary_Function(BSON_Binary):

    _subcode = b"\01"

    @typecheck
    def __init__(self, value: bytes):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_Function)

###############################################################################

class BSON_Binary_UUID(BSON_Binary):

    _subcode = b"\03"

    @typecheck
    def __init__(self, value: bytes):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_UUID)

###############################################################################

class BSON_Binary_MD5(BSON_Binary):

    _subcode = b"\05"

    @typecheck
    def __init__(self, value: bytes):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_MD5)

###############################################################################

class BSON_Binary_UserDefined(BSON_Binary):

    _subcode = b"\x80"

    @typecheck
    def __init__(self, value: bytes):
        self._value = value

BSON_Binary._register_subtype(BSON_Binary_UserDefined)

###############################################################################

class BSON_ObjectId(BSON_Value):

    _code = b"\x07"

    @typecheck
    def __init__(self, value: lambda x: (isinstance(x, bytes) and len(x) == 12) or (isinstance(x, str) and len(x) == 24)):
        if isinstance(value, str):
            self._value = unhexlify(value.encode("ascii"))
        else:
            self._value = value

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

    @typecheck
    def __init__(self, value: bool):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        stream.write(self._value and b"\x01" or b"\x00")

    @classmethod
    def parse(cls, stream):
        b = cls._read(stream, 1)[0]
        if b not in (0, 1):
            raise BSON_ParsingError("incorrect boolean value at offset {0:d}".\
                                    format(stream.tell())) # tested
        return cls(b == 1)

BSON_Value._register_type(BSON_Boolean)

###############################################################################

class BSON_Datetime(BSON_Value):

    _code = b"\x09"

    @typecheck
    def __init__(self, value: datetime):
        self._value = value

    def _py_value(self):
        return self._value

    def serialize(self, stream):
        epoch_ms = int(timegm(self._value.utctimetuple()) * 1000 + self._value.microsecond // 1000)
        BSON_Int64(epoch_ms).serialize(stream)

    @classmethod
    def parse(cls, stream):
        epoch_ms = BSON_Int64.parse(stream).value
        return cls(datetime.utcfromtimestamp(epoch_ms / 1000.0))

BSON_Value._register_type(BSON_Datetime)

###############################################################################

class BSON_Null(BSON_Value):

    _code = b"\x0a"

    @typecheck
    def __init__(self, value: type(None)):
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

    @typecheck
    def __init__(self, value: (str, str)):
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

    @typecheck
    def __init__(self, value: str):
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

    @typecheck
    def __init__(self, value: str):
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

    @typecheck
    def __init__(self, value: (str, BSON_Document)):
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

    @typecheck
    def __init__(self, value: lambda x: isinstance(x, int) and -2**31 <= x <= 2**31-1):
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

    @typecheck
    def __init__(self, value: int):
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

    @typecheck
    def __init__(self, value: lambda x: isinstance(x, int) and -2**63 <= x <= 2**63-1):
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

_py_to_bs = \
{
    float : lambda f: BSON_Double(f),
    str   : lambda s: BSON_String(s),
    dict  : lambda d: BSON_Document(odict((str(k), py_to_bs(v)) for k, v in d.items())),
    list  : lambda l: BSON_Array([ py_to_bs(v) for v in l ]),
    int   : lambda i: BSON_Int32(i) if -2**31 <= i <= 2**31-1 else BSON_Int64(i) if -2**63 <= i <= 2**63-1 else _py_no_bs(i),
    bytes : lambda b: BSON_Binary_Generic(b),
    bool  : lambda b: BSON_Boolean(b),
    datetime : lambda dt: BSON_Datetime(dt),
    type(None) : lambda n: BSON_Null(n),
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

@typecheck
def serialize_to_bytes(document: dict) -> bytes:
    stream = BytesIO()
    serialize_to_stream(document, stream)
    return stream.getvalue()

@typecheck
def serialize_to_stream(document: dict, stream: with_attr("write", "flush")):
    py_to_bs(document).serialize(stream)
    stream.flush()

###############################################################################

@typecheck
def parse_bytes(serialized: bytes) -> odict:
    stream = BytesIO(serialized)
    return parse_stream(stream)

@typecheck
def parse_stream(stream: with_attr("read", "tell")) -> odict:
    return bs_to_py(BSON_Document.parse(stream))


###############################################################################
# EOF
