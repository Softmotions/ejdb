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

        ejdb.remove("foocoll", doc["_id"])
        ldoc = ejdb.load("foocoll", doc["_id"])
        self.assertTrue(ldoc is None)
        ejdb.sync()

        ejdb.ensureCollection("ecoll1", records=90000, large=False)
        ejdb.dropCollection("ecoll1", prune=True)

    def test2(self):
        pass

    @classmethod
    def tearDownClass(cls):
        if cls._ejdb:
            cls._ejdb.close()
            cls._ejdb = None

if __name__ == '__main__':
    unittest.main()




