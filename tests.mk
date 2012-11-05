
check:
	- mkdir -p var
	make -C ./tcejdb check-ejdb
	nodeunit ./node/tests

check-all:
	- mkdir -p var
	make -C ./tcejdb check
	nodeunit ./node/tests


.PHONY: check check-all