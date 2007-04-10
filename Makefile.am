# $Id: Makefile.am,v 1.7 2007-04-10 08:48:55 adam Exp $

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src doc

EXTRA_DIST = README NEWS LICENSE buildconf.sh Doxyfile.in m4/yaz.m4 

dist-hook:
	if test -x /usr/bin/cvs2cl -a -d CVS; then cvs2cl ; cp ChangeLog $(distdir); fi
	cp -r $(srcdir)/etc $(distdir)
	cvs=`find $(distdir)/etc -name CVS -type d -print`; if test "$$cvs"; then rm -r $$cvs; fi
	cp -r $(srcdir)/www $(distdir)
	cvs=`find $(distdir)/www -name CVS -type d -print`; if test "$$cvs"; then rm -r $$cvs; fi


cvs2cl:
	cvs2cl


dox:
	doxygen

showdox:
	doxygen
	galeon file:///`pwd`/$(top_srcdir)/dox/html/index.html &

.PHONY: cvs2cl dox showdox
