CURDIR:=$(shell pwd)
SRCDIR:=$(CURDIR)/src
export LIBDIR:=$(CURDIR)/lib
export TMPDIR:=$(CURDIR)

all : lib bin

.PHONY: lib
lib : 
	mkdir -p $(TMPDIR)
	make -C $(LIBDIR)

.PHONY: bin
bin : lib
	make -C $(SRCDIR)

.PHONY: clean
clean :
	make clean -C lib
	make clean -C src