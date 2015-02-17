
CC=gcc
CPPFLAGS = -I. -I../../../ -D_GNU_SOURCE=1
CFLAGS = -std=c99 -Wall -fPIC -pedantic -O2
LDFLAGS = -L../../
LIBS =

sample1 : sample1.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ltcejdb $(LIBS)

run : sample1
	export LD_LIBRARY_PATH=../.. && ./sample1

