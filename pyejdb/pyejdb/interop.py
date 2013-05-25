import sys
import codecs

if sys.version < '3':
    def b(x):
        return x

    def u(x):
        return codecs.unicode_escape_decode(x)[0]
else:
    def b(x):
        return codecs.latin_1_encode(x)[0]

    def u(x):
        return x
