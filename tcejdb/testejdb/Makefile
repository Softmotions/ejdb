
CC=gcc
CPPFLAGS = -I. -I.. -DDEBUG -D_DEBUG -D_GNU_SOURCE=1 -D_REENTRANT -D__EXTENSIONS__
CFLAGS = -std=c99 -Wall -fPIC -pedantic -fsigned-char -g -O0
LDFLAGS = -L. -L..
CMDLDFLAGS =
LIBS = -lz -lpthread -lm -lc -lcunit
RUNENV = LD_LIBRARY_PATH=.:.. DYLD_LIBRARY_PATH=.:..

TESTS = t1 t2 t3

check : all check-t1 check-t2 check-t3;

check-valgrind :
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check

check-valgrind-t2 :
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check-t2

check-valgrind-t3 :
	make RUNCMD="valgrind --tool=memcheck --leak-check=full --error-exitcode=1" check-t3

check-t1 :
	$(RUNENV) $(RUNCMD) ./t1

check-t2 :
	$(RUNENV) $(RUNCMD) ./t2

check-t3 :
	$(RUNENV) $(RUNCMD) ./t3


all : $(TESTS)

clean :
	rm -rf *.o $(TESTS) db* *.vlog

t1 : t1.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ltcejdb $(CMDLDFLAGS) $(LIBS)

t2 : t2.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ltcejdb $(CMDLDFLAGS) $(LIBS)

t3 : t3.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ltcejdb $(CMDLDFLAGS) $(LIBS)


.PHONY : all clean check run-t1 run-t2 run-t3

