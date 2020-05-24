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
LIBS = -l$(XORSATLIB) -lm -lpthread
LDFLAGS = -Llib
CFLAGS = -std=gnu99 $(DBG) $(OPT) $(INCLUDES)
AR = ar r
RANLIB = ranlib

all: depend lib/lib$(XORSATLIB).a

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

OBJECTS_CTHREADPOOL = lib/C-Thread-Pool/obj/*.o

lib/C-Thread-Pool/obj/thpool.o: lib/C-Thread-Pool/thpool.c
	@echo "Compiling "$<""
	@[ -d lib/C-Thread-Pool/obj ] || mkdir -p lib/C-Thread-Pool/obj
	@$(CC) $(CFLAGS) -c -Wno-implicit-function-declaration $< -o $@

lib/bitvector/lib/libbitvector.a:
	@cd lib/bitvector && $(MAKE)

$(OBJECTS): obj/%.o : src/%.c Makefile
	@echo "Compiling "$<""
	@[ -d obj ] || mkdir -p obj
	@$(CC) $(CFLAGS) -c $< -o $@

lib/lib$(XORSATLIB).a: $(OBJECTS) Makefile lib/bitvector/lib/libbitvector.a lib/C-Thread-Pool/obj/thpool.o
	@echo "Creating "$@""
	@[ -d lib ] || mkdir -p lib
	@rm -f $@
	@cp lib/bitvector/lib/libbitvector.a $@
	@$(AR) $@ $(OBJECTS_CTHREADPOOL)
	@$(AR) $@ $(OBJECTS)
	@$(RANLIB) $@

test/test: test/test.c lib/lib$(XORSATLIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) test/test.c -o test/test $(LIBS)

clean:
	cd lib/bitvector && $(MAKE) clean
	rm -rf $(OBJECTS_CTHREADPOOL)
	rm -rf *~ */*~ $(OBJECTS) ./.depend test/test *.dSYM test/test.dSYM XORSATFilter.tar.gz filter.xor lib/lib$(XORSATLIB).a obj

edit:
	emacs -nw $(SOURCES) $(HEADERS) $(EXTRAS)
