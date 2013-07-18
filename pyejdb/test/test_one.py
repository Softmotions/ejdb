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

from datetime import datetime
import sys

PY3 = sys.version_info[0] == 3

import unittest
from pyejdb import bson
import pyejdb

if PY3:
    from io import StringIO as strio
else:
    from io import BytesIO as strio



class TestOne(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestOne, self).__init__(*args, **kwargs)
        #super().__init__(*args, **kwargs)

    _ejdb = None

    @classmethod
    def setUpClass(cls):
        print("pyejdb version: %s" % pyejdb.version)
        print("libejdb_version: %s" % pyejdb.libejdb_version)
        cls._ejdb = pyejdb.EJDB("testdb", pyejdb.DEFAULT_OPEN_MODE | pyejdb.JBOTRUNC)

    def test(self):
        ejdb = TestOne._ejdb
        self.assertEqual(ejdb.isopen, True)
        doc = {"foo": "bar", "foo2": 2}
        ejdb.save("foocoll", doc)
        self.assertEqual(type(doc["_id"]).__name__, "str" if PY3 else "unicode")
        ldoc = ejdb.load("foocoll", doc["_id"])
        self.assertIsInstance(ldoc, dict)
        self.assertEqual(doc["_id"], ldoc["_id"])
        self.assertEqual(doc["foo"], ldoc["foo"])
        self.assertEqual(doc["foo2"], ldoc["foo2"])

        cur = ejdb.find("foocoll", {"foo": "bar"}, hints={"$fields": {"foo2": 0}})
        self.assertEqual(len(cur), 1)
        d = cur[0]
        self.assertTrue(d is not None)
        self.assertEqual(d["_id"], ldoc["_id"])

        with ejdb.find("foocoll") as cur2:
            d = cur2[0]
            self.assertTrue(d is not None)
            self.assertEqual(d["_id"], ldoc["_id"])

        self.assertEqual(ejdb.findOne("foocoll")["foo"], "bar")
        self.assertTrue(ejdb.findOne("foocoll2") is None)

        self.assertEqual(ejdb.count("foocoll"), 1)
        self.assertEqual(ejdb.count("foocoll2"), 0)

        ejdb.ensureStringIndex("foocoll", "foo")
        cur = ejdb.find("foocoll", {"foo": "bar"}, hints={"$fields": {"foo2": 0}})
        self.assertEqual(len(cur), 1)

        ejdb.remove("foocoll", doc["_id"])
        ldoc = ejdb.load("foocoll", doc["_id"])
        self.assertTrue(ldoc is None)
        ejdb.sync()

        ejdb.ensureCollection("ecoll1", records=90000, large=False)
        ejdb.dropCollection("ecoll1", prune=True)


    def test2(self):
        ejdb = TestOne._ejdb
        self.assertEqual(ejdb.isopen, True)
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
        ejdb.save("parrots", *[parrot1, None, parrot2])
        self.assertEqual(type(parrot1["_id"]).__name__, "str" if PY3 else "unicode")
        self.assertEqual(type(parrot2["_id"]).__name__, "str" if PY3 else "unicode")
        p2 = ejdb.load("parrots", parrot2["_id"])
        self.assertEqual(p2["_id"], parrot2["_id"])

        cur = ejdb.find("parrots")
        self.assertEqual(len(cur), 2)
        self.assertEqual(len(cur[1:]), 1)
        self.assertEqual(len(cur[2:]), 0)

        cur = ejdb.find("parrots",
                {"name": bson.BSON_Regex(("(grenny|bounty)", "i"))},
                        hints={"$orderby": [("name", 1)]})
        self.assertEqual(len(cur), 2)
        self.assertEqual(cur[0]["name"], "Bounty")
        self.assertEqual(cur[0]["age"], 15)

        cur = ejdb.find("parrots", {}, {"name": "Grenny"}, {"name": "Bounty"},
                        hints={"$orderby": [("name", 1)]})
        self.assertEqual(len(cur), 2)

        cur = ejdb.find("parrots", {}, {"name": "Grenny"},
                        hints={"$orderby": [("name", 1)]})
        self.assertEqual(len(cur), 1)

        sally = {
            "name": "Sally",
            "mood": "Angry",
            }
        molly = {
            "name": "Molly",
            "mood": "Very angry",
            "secret": None
        }
        ejdb.save("birds", *[sally, molly])

        logbuf = strio()
        ejdb.find("birds", {"name": "Molly"}, log=logbuf)
        #print("LB=%s" % logbuf.getvalue())
        self.assertTrue(logbuf.getvalue().find("RUN FULLSCAN") != -1)

        ejdb.ensureStringIndex("birds", "name")

        logbuf = strio()
        ejdb.find("birds", {"name": "Molly"}, log=logbuf)
        self.assertTrue(logbuf.getvalue().find("MAIN IDX: 'sname'") != -1)
        self.assertTrue(logbuf.getvalue().find("RUN FULLSCAN") == -1)

        ##print("dbmeta=%s" % ejdb.dbmeta())
        bar = {
            "foo": "bar"
        }
        self.assertEqual(ejdb.isactivetx("bars"), False)
        ejdb.begintx("bars")
        self.assertEqual(ejdb.isactivetx("bars"), True)
        ejdb.save("bars", bar)
        self.assertTrue(bar["_id"] is not None)
        ejdb.abortx("bars")
        self.assertTrue(ejdb.load("bars", bar["_id"]) is None)

        ejdb.begintx("bars")
        ejdb.save("bars", bar)
        self.assertTrue(ejdb.load("bars", bar["_id"]) is not None)
        self.assertEqual(ejdb.isactivetx("bars"), True)
        ejdb.commitx("bars")
        self.assertEqual(ejdb.isactivetx("bars"), False)
        self.assertTrue(ejdb.load("bars", bar["_id"]) is not None)

        ejdb.update("upsertcoll",
                {"foo": "bar", "$upsert": {"foo": "bar"}})
        self.assertTrue(ejdb.findOne("upsertcoll", {"foo": "bar"}) is not None)


        cmd = {
            "ping" : {}
        }
        cmdret = ejdb.command(cmd)
        self.assertIsNotNone(cmdret)
        self.assertEquals(cmdret["log"], "pong")


    @classmethod
    def tearDownClass(cls):
        if cls._ejdb:
            cls._ejdb.close()
            cls._ejdb = None

if __name__ == '__main__':
    unittest.main()




