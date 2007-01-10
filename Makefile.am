AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src doc

EXTRA_DIST = README PROTOCOL m4/yaz.m4 

dist-hook:
	if test -x /usr/bin/cvs2cl -a -d CVS; then cvs2cl ; fi
	cp -r $(srcdir)/etc $(distdir)
	cvs=`find $(distdir)/etc -name CVS -type d -print`; if test "$$cvs"; then rm -r $$cvs; fi
	cp -r $(srcdir)/www $(distdir)
	cvs=`find $(distdir)/www -name CVS -type d -print`; if test "$$cvs"; then rm -r $$cvs; fi
