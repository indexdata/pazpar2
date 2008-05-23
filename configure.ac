## -*- Autoconf -*-
## This file is part of Pazpar2.
## Process this file with autoconf to produce a configure script.

# Autoconf and automake setup
AC_PREREQ(2.60)
AC_INIT([pazpar2],[1.0.9],[pazpar2-help@indexdata.dk])

AC_CONFIG_HEADERS(src/config.h)

AC_CONFIG_SRCDIR([configure.ac]) 
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.9])

AC_PROG_CC
AC_PROG_RANLIB

AC_LANG(C)

YAZ_INIT([static threads],[3.0.25])
if test -z "$YAZLIB"; then
	AC_MSG_ERROR([YAZ development libraries missing])
fi
YAZ_DOC

AC_CHECK_HEADERS([sys/time.h sys/socket.h unistd.h netinet/in.h netdb.h arpa/inet.h])
AC_CHECK_FUNCS([getaddrinfo])
AC_CHECK_ICU([3.6],[ICU_CPPFLAGS="$ICU_CPPFLAGS -D HAVE_ICU=1"])
AC_CONFIG_FILES([
	Doxyfile
	Makefile
	src/Makefile
	test/Makefile
	js/Makefile
	doc/Makefile
	doc/local.ent
	doc/common/Makefile
	doc/common/print.dsl

])

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
  Automake:                   ${AUTOMAKE}
  Archiver:                   ${AR}
  Ranlib:                     ${RANLIB}
  YAZ Version:                ${YAZVERSION}
  YAZ Include:                ${YAZINC}
  YAZ La Lib:                 ${YAZLALIB}
  YAZ Lib:                    ${YAZLIB}
  ICU Version:                ${ICU_VERSION}
  ICU Include:                ${ICU_CPPFLAGS}
  ICU Lib:                    ${ICU_LIBS}
  Bugreport:                  ${PACKAGE_BUGREPORT}

------------------------------------------------------------------------"

dnl Local Variables:
dnl mode:shell-script
dnl sh-indentation:2
dnl sh-basic-offset: 4
dnl End:
