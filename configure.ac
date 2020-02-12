AC_PREREQ([2.59])
AC_INIT([kst], [1.0.0], [kyle@imap.cc],
	[libdirutils], [https://hg.sr.ht/~kisom/kst])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects])
AC_CONFIG_SRCDIR([docs/srm.1])
AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile test/Makefile])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

NO_CUNIT_MSG="
    ==============================================
    Warning: CUnit was not found; will not be able
    to run unit tests!
    ==============================================
"
AC_SEARCH_LIBS([CU_initialize_registry], [cunit],
	       [], [AC_MSG_WARN($NO_CUNIT_MSG)])
AC_OUTPUT
