#RUNENV = LD_LIBRARY_PATH=.:.. DYLD_LIBRARY_PATH=.:..

check:
	$(RUNENV) $(RUNCMD) lua ./t1.lua
	make clean

clean:
	rm -f ./testdb*

