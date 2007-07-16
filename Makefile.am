# $Id: Makefile.am,v 1.14 2007-07-16 12:31:09 adam Exp $

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test js doc

EXTRA_DIST = README NEWS LICENSE buildconf.sh Doxyfile.in m4/yaz.m4 

dist-hook:
	if test -x /usr/bin/cvs2cl -a -d CVS; then cvs2cl ; cp ChangeLog $(distdir); fi
	for d in etc etc/settings www/test1 www/jsdemo debian; do \
		mkdir -p $(distdir)/$$d; \
		for f in $(srcdir)/$$d/*; do \
			test -f $$f && cp $$f $(distdir)/$$d; \
		done; \
	done; exit 0

cvs2cl:
	cvs2cl


dox:
	doxygen

showdox:
	doxygen
	galeon file:///`pwd`/$(top_srcdir)/dox/html/index.html &

.PHONY: cvs2cl dox showdox
