
tests :
	- mkdir -p var
	##make -C ./tcejdb check-ejdb
	nodeunit ./node/tests


.PHONY: tests