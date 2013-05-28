import sys
import unittest


names = ['one']

def test_suite():
    import pyejdb
    # Go
    modules = ['test.test_%s' % n for n in names]
    return unittest.defaultTestLoader.loadTestsFromNames(modules)


def main():
    unittest.main(module=__name__, defaultTest='test_suite', argv=sys.argv[:1])

if __name__ == '__main__':
    main()