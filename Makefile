EXTRAS = Makefile DISCLAIMER.md LICENSE.md README.md LICENSE.xxHash	\
test/test.c

HEADERS = lib/c_list_types/include/c_list_types.h		\
include/list_types.h include/xorsat_hashes.h			\
include/xorsat_metadata.h include/MurmurHash3.h			\
include/xorsat_blocks.h include/xorsat_solve.h			\
include/xorsat_immir_wrap.h include/xorsat_serial.h		\
include/xorsat_filter.h include/immir.h

SOURCES = src/list_types.c src/xorsat_hashes.c src/xorsat_metadata.c	\
src/MurmurHash3.c src/xorsat_blocks.c src/xorsat_solve.c		\
src/xorsat_immir_wrap.c src/xorsat_serial.c src/xorsat_build.c		\
src/xorsat_query.c src/immir.c

OBJECTS = $(SOURCES:src/%.c=obj/%.o)

XORSATLIB = xorsatfilter
CC = gcc
DBG = #-g -Wall -fstack-protector-all -pedantic
OPT = -march=native -O3 -DNDEBUG -ffast-math -fomit-frame-pointer -finline-functions
INCLUDES = -Iinclude
LIBS = -l$(XORSATLIB) -lm
LDFLAGS = -Llib
CFLAGS = -std=gnu99 $(DBG) $(OPT) $(INCLUDES) -fopenmp
AR = ar r
RANLIB = ranlib

all: depend lib/bitvector/lib/libbitvector.a lib/lib$(XORSATLIB).a

depend: .depend
.depend: $(SOURCES)
	@echo "Building dependencies" 
ifneq ($(wildcard ./.depend),)
	@rm -f "./.depend"
endif
	@$(CC) $(CFLAGS) -MM $^ > .depend
# Make .depend use the 'obj' directory
	@sed -i.bak -e :a -e '/\\$$/N; s/\\\n//; ta' .depend
	@sed -i.bak 's/^/obj\//' .depend
	@rm -f .depend.bak
-include .depend

lib/bitvector/lib/libbitvector.a:
	@cd lib/bitvector && $(MAKE)

$(OBJECTS): obj/%.o : src/%.c Makefile
	@echo "Compiling "$<""
	@[ -d obj ] || mkdir -p obj
	@$(CC) $(CFLAGS) -c $< -o $@

lib/lib$(XORSATLIB).a: $(OBJECTS) Makefile lib/bitvector/lib/libbitvector.a
	@echo "Creating "$@""
	@[ -d lib ] || mkdir -p lib
	@rm -f $@
	@cp lib/bitvector/lib/libbitvector.a $@
	@$(AR) $@ $(OBJECTS)
	@$(RANLIB) $@

test/test: test/test.c lib/lib$(XORSATLIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) test/test.c -o test/test $(LIBS)

.PHONY: dist
dist: $(SOURCES) $(HEADERS) $(EXTRAS)
	rm -rf xor_filter.tar.gz
	[ -d xor_filter         ] || mkdir -p xor_filter
	[ -d xor_filter/obj     ] || mkdir -p xor_filter/obj
	[ -d xor_filter/lib     ] || mkdir -p xor_filter/lib
	[ -d xor_filter/src     ] || mkdir -p xor_filter/src
	[ -d xor_filter/scripts ] || mkdir -p xor_filter/scripts
	rsync -R $(SOURCES) $(HEADERS) $(EXTRAS) xor_filter
	COPYFILE_DISABLE=1 tar cvzf xor_filter.tar.gz xor_filter
	rm -rf xor_filter

clean:
	cd lib/bitvector && $(MAKE) clean
	rm -rf *~ */*~ $(OBJECTS) ./.depend test/test *.dSYM test/test.dSYM xor_filter.tar.gz filter.xor lib/lib$(XORSATLIB).a obj

edit:
	emacs -nw $(SOURCES) $(HEADERS) $(EXTRAS)
