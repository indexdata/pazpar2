# This file is part of Pazpar2.
AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test js doc

EXTRA_DIST = $(PACKAGE).spec IDMETA README NEWS LICENSE m4/id-config.sh \
	buildconf.sh Doxyfile.in m4/yaz.m4 \
	win/makefile win/version.nsi win/version.nsi.in win/license.txt

dist-hook:
	if test -x /usr/bin/git -a -d .git; then git log >ChangeLog ; cp ChangeLog $(distdir); fi
	for d in etc etc/settings etc/services www/test1 www/jsdemo www/iphone; do \
		mkdir -p $(distdir)/$$d; cp -r $$d/* $(distdir)/$$d; \
	done; rm -f $(distdir)/etc/pazpar2.cfg; exit 0
	mkdir $(distdir)/rpm
	-cp $(srcdir)/rpm/* $(distdir)/rpm

.PHONY:debian
debian:
	dpkg-buildpackage -rfakeroot
