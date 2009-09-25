# This file is part of Pazpar2.
AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test js doc

EXTRA_DIST = README NEWS LICENSE buildconf.sh Doxyfile.in m4/yaz.m4 \
	win/makefile win/version.nsi win/version.nsi.in win/license.txt

dist-hook:
	if test -x /usr/bin/git -a -d .git; then git log >ChangeLog ; cp ChangeLog $(distdir); fi
	for d in etc etc/settings www/test1 www/jsdemo; do \
		mkdir -p $(distdir)/$$d; \
		for f in $(srcdir)/$$d/*; do \
			test -f $$f && cp $$f $(distdir)/$$d; \
		done; \
	done; rm -f $(distdir)/etc/pazpar2.cfg; exit 0

.PHONY:debian
debian:
	dpkg-buildpackage -rfakeroot
