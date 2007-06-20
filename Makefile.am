# $Id: Makefile.am,v 1.13 2007-06-20 14:11:41 adam Exp $

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test js doc

EXTRA_DIST = README NEWS LICENSE buildconf.sh Doxyfile.in m4/yaz.m4 

dist-hook:
	if test -x /usr/bin/cvs2cl -a -d CVS; then cvs2cl ; cp ChangeLog $(distdir); fi
	cp -r $(srcdir)/etc $(distdir)
	cvs=`find $(distdir)/etc -name CVS -type d -print`; if test "$$cvs"; then rm -r $$cvs; fi
	mkdir -p $(distdir)/www/test1
	for f in $(srcdir)/www/test1/*; do \
		test -f $$f && cp $$f $(distdir)/www/test1; \
	done; exit 0
	mkdir -p $(distdir)/www/jsdemo
	for f in $(srcdir)/www/jsdemo/*; do \
		test -f $$f && cp $$f $(distdir)/www/jsdemo; \
	done; exit 0
	mkdir $(distdir)/debian
	for f in $(srcdir)/debian/*; do \
		test -f $$f && cp $$f $(distdir)/debian; \
	done; exit 0

cvs2cl:
	cvs2cl


dox:
	doxygen

showdox:
	doxygen
	galeon file:///`pwd`/$(top_srcdir)/dox/html/index.html &

.PHONY: cvs2cl dox showdox
