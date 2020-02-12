CFLAGS :=	-pedantic -Wall -Werror -Wextra -O2 -std=c99 -g

lib_LIBRARIES :=	libdirutils.a libiniparser.a
nobase_include_HEADERS :=	kst/dirutils.h	\
				kst/iniparser.h
dist_noinst_HEADERS =		kst/dirlist.h
bin_PROGRAMS =		srm


## programs
srm_SOURCES = srm.c


## libraries
libdirutils_a_SOURCES = dirutils.c dirlist.c dirwalk.c dirlist.h
libiniparser_a_SOURCES = iniparser.c



