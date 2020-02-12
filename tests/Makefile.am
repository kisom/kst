AM_LDFLAGS = -L/usr/local/lib -lcunit
AM_CFLAGS = -Wall -g -I/usr/local/include -I../src -O0 

check_PROGRAMS = dirlist_test dirutils_test iniparser-test
dirlist_test_SOURCES = dirlist_test.c ../src/dirlist.c
dirutils_test_SOURCES = dirutils_test.c ../src/dirlist.c ../src/dirutils.c
iniparser_test_SOURCES = iniparser_test.c ../src/iniparser.c


