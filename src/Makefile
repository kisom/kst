BINS :=	srm
EDS :=	ke kte
LIBS := libdirutils.a libiniparser.a

LDFLAGS :=
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O0 -std=c99 -g -I../include/

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

iniparser-test: iniparser_test.c libiniparser.a
	$(CC) $(CFLAGS) -o $@ iniparser_test.c libiniparser.a

libdirutils.a: dirlist.o dirutils.o dirwalk.o
	$(AR) -rcs $@ dirlist.o dirutils.o dirwalk.o

dirlist-test: dirlist_test.c libdirutils.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dirlist_test.c libdirutils.a

dirutils-test: dirutils_test.c libdirutils.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dirutils_test.c libdirutils.a

