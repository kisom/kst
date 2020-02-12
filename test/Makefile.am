AM_LDFLAGS = -L/usr/local/include -lcunit -L../src/
AM_CFLAGS = -Wall -g -I/usr/local/include -I../src -O0 

check_PROGRAMS = dirlist_test dirutils_test
dirlist_test_CFLAGS = $(AM_CFLAGS)
dirlist_test_SOURCES = dirlist_test.c 
dirutils_test_CFLAGS = $(AM_CFLAGS)
dirutils_test_SOURCES = dirutils_test.c

