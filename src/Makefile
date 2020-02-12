BINS :=	srm
EDS :=	ke kte
LIBS := libdirutils.a libiniparser.a

LDFLAGS :=	-L. -L/usr/local/lib
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O0 -std=c99 -g
CFLAGS +=	-I../include/ -I/usr/local/include/

.PHONY: all
all: $(BINS) $(EDS) $(LIBS)

.PHONY: clean
clean:
	rm -f $(BINS) $(EDS) $(LIBS) *.o *.core


ke kte:
	cd ../$@ && make && cp $@ ../src

srm: srm.c
	$(CC) $(CFLAGS) -o $@ srm.c

libiniparser.a: iniparser.o
	$(AR) -rcs $@ iniparser.o

libdirutils.a: dirlist.o dirutils.o dirwalk.o
	$(AR) -rcs $@ dirlist.o dirutils.o dirwalk.o


.PHONY: tests
tests: iniparser-test dirlist-test dirutils-test

iniparser-test: iniparser_test.c libiniparser.a
	$(CC) $(CFLAGS) -o $@ iniparser_test.c libiniparser.a

dirlist-test: dirlist_test.c libdirutils.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dirlist_test.c libdirutils.a -lcunit

dirutils-test: dirutils_test.c libdirutils.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dirutils_test.c libdirutils.a -lcunit
