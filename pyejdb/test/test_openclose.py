import unittest
import pyejdb

class TestOpenClose(unittest.TestCase):
    def setUp(self):
        pass

    def test_open(self):
        self.ejdb = pyejdb.EJDB("testdb")

    def tearDown(self):
        if self.ejdb:
            self.ejdb.close()
            self.ejdb = None

if __name__ == '__main__':
    unittest.main()




