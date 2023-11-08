CURDIR:=$(shell pwd)
SORTDIR:=$(CURDIR)/sort
export LIBDIR:=$(CURDIR)/utils
export TMPDIR:=$(CURDIR)

.PHONY:all
all :  lib sort

.PHONY:lib
lib :
	make -C $(LIBDIR)

.PHONY:sort
sort : lib
	make -C $(SORTDIR)

.PHONY:clean
clean:
	make clean -C $(SORTDIR)
	make clean -C $(LIBDIR)
