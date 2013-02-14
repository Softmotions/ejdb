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

import unittest
import pyejdb

class TestOne(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

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
        self.assertIsInstance(doc["_id"], str)
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

        print("dbmeta=%s" % ejdb.dbmeta())


    def test2(self):
        pass

    @classmethod
    def tearDownClass(cls):
        if cls._ejdb:
            cls._ejdb.close()
            cls._ejdb = None

if __name__ == '__main__':
    unittest.main()




