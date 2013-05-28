import sys
import codecs

if sys.version < '3':
    def b(x):
        return x

    def u(x):
        return unicode(x, "unicode_escape")
else:
    def b(x):
        return x.encode("latin-1")

    def u(x):
        return x
