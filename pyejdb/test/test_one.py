import unittest
import pyejdb

class TestOne(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    _ejdb = None

    @classmethod
    def setUpClass(cls):
        cls._ejdb = pyejdb.EJDB("testdb")

    def test_save1(self):
        ejdb = TestOne._ejdb
        doc1 = {"foo" : "bar", "foo2" : 2}
        ejdb.save("foocoll", doc1)

    @classmethod
    def tearDownClass(cls):
        if cls._ejdb:
            cls._ejdb.close()
            cls._ejdb = None

if __name__ == '__main__':
    unittest.main()




