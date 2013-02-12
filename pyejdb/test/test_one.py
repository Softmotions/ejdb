import unittest
import pyejdb

class TestOne(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    _ejdb = None

    @classmethod
    def setUpClass(cls):
        cls._ejdb = pyejdb.EJDB("testdb", pyejdb.DEFAULT_OPEN_MODE | pyejdb.JBOTRUNC)

    def test_saveload(self):
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
        ejdb.remove("foocoll", doc["_id"])
        ldoc = ejdb.load("foocoll", doc["_id"])
        self.assertTrue(ldoc is None)

    @classmethod
    def tearDownClass(cls):
        if cls._ejdb:
            cls._ejdb.close()
            cls._ejdb = None

if __name__ == '__main__':
    unittest.main()




