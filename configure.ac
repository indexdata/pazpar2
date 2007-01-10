## -*- Autoconf -*-
## Process this file with autoconf to produce a configure script.

# Autoconf and automake setup
AC_PREREQ(2.59)
AC_INIT([pazpar2],[0.1],[pazpar2-help@indexdata.dk])

AC_CONFIG_HEADERS(src/cconfig.h)

AC_CONFIG_SRCDIR([configure.ac]) 
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.8])

AC_PROG_CC

AC_LANG(C)

YAZ_INIT(libxml2,2.1.44)
if test -z "$YAZLIB"; then
	AC_MSG_ERROR([YAZ development libraries missing])
fi
YAZ_DOC

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	doc/Makefile
	doc/local.ent
	doc/common/Makefile
	doc/common/print.dsl

])

AC_OUTPUT
