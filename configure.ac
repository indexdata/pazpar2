## -*- Autoconf -*-
## This file is part of Pazpar2.
## Copyright (C) Index Data
## Process this file with autoconf to produce a configure script.

# Autoconf and automake setup
AC_PREREQ([2.69])
AC_INIT([pazpar2],[m4_esyscmd([. ./IDMETA; printf $VERSION])],[info@indexdata.com])

AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([config])

AC_DEFINE([_POSIX_C_SOURCE],[200809L],[Enable POSIX])

AM_INIT_AUTOMAKE([1.9])

AC_PROG_CC
AC_PROG_RANLIB

AC_C_INLINE

YAZ_INIT([static icu],[5.18.0])
if test -z "$YAZLIB"; then
	AC_MSG_ERROR([YAZ development libraries missing])
fi
case $YAZINC in
  *YAZ_HAVE_XSLT=1*) ;;
  *) AC_MSG_ERROR([YAZ not compiled with Libxslt support]) ;;
esac

YAZ_DOC

AC_SEARCH_LIBS([log],[m])
AC_CHECK_HEADERS([sys/time.h sys/socket.h unistd.h netdb.h])
checkBoth=0
AC_CHECK_FUNC([connect])
if test "$ac_cv_func_connect" = "no"; then
    AC_CHECK_LIB([socket],[main], [LIBS="$LIBS -lsocket"], [checkBoth=1])
fi
if test "$checkBoth" = "1"; then
    oldLibs=$LIBS
    LIBS="$LIBS -lsocket -lnsl"
    AC_CHECK_FUNC([accept], , [LIBS=$oldLibs])
fi
AC_CHECK_FUNC([gethostbyname], ,[AC_CHECK_LIB(nsl, main, [LIBS="$LIBS -lnsl"])])
AC_CHECK_FUNCS([getaddrinfo getrlimit])

if test -d ${srcdir}/.git; then
	sha=`git show --pretty=format:%H|head -1`
else
	sha=`head -1 ${srcdir}/ChangeLog|awk '{print $2}'`
fi
AC_DEFINE_UNQUOTED([PAZPAR2_VERSION_SHA1], ["$sha"], [Git SHA1])

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	src/Makefile
	test/Makefile
	js/Makefile
	etc/xsl/Makefile
	doc/Makefile
	doc/local0.ent
	doc/common/Makefile
	doc/common/print.dsl
	win/version.nsi
])
AC_CONFIG_COMMANDS([default],[
	diff doc/local.ent doc/local0.ent >/dev/null 2>/dev/null \
	|| cp doc/local0.ent doc/local.ent
],[])
AC_OUTPUT

echo \
"------------------------------------------------------------------------

  Package:                    ${PACKAGE}
  Version:                    ${VERSION}
  Source code location:       ${srcdir}
  C Preprocessor:             ${CPP}
  C Preprocessor flags:       ${CPPFLAGS}
  C Compiler:                 ${CC}
  C Compiler flags:           ${CFLAGS}
  Linker flags:               ${LDFLAGS}
  Linked libs:                ${LIBS}
  Host System Type:           ${host}
  Install path:               ${prefix}
  YAZ Version:                ${YAZVERSION}
  YAZ Include:                ${YAZINC}
  YAZ Lib:                    ${YAZLIB}
  Bugreport:                  ${PACKAGE_BUGREPORT}

------------------------------------------------------------------------"

dnl Local Variables:
dnl mode:shell-script
dnl sh-indentation:2
dnl sh-basic-offset: 4
dnl End:
