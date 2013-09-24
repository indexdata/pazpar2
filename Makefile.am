# This file is part of Pazpar2.
AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test js doc

EXTRA_DIST = $(PACKAGE).spec IDMETA README NEWS LICENSE m4/id-config.sh \
	buildconf.sh Doxyfile.in m4/yaz.m4 \
	win/makefile win/version.nsi win/version.nsi.in win/license.txt

dist-hook:
	if test -x /usr/bin/git -a -d .git; then git log >ChangeLog ; cp ChangeLog $(distdir); fi
	for d in etc etc/settings etc/services www/test1 www/jsdemo; do \
		mkdir -p $(distdir)/$$d; cp -r $$d/* $(distdir)/$$d; \
	done; rm -f $(distdir)/etc/pazpar2.cfg; exit 0
	mkdir $(distdir)/rpm
	-cp $(srcdir)/rpm/* $(distdir)/rpm

JSDISTDIR = pazpar2-js-$(VERSION)
js-dist:
	mkdir -p $(JSDISTDIR)
	cp LICENSE js/*js $(JSDISTDIR)
	echo $(VERSION) > $(JSDISTDIR)/VERSION
	tar cvfz pazpar2-js-$(VERSION).tar.gz $(JSDISTDIR)
	rm -fr $(JSDISTDIR)

.PHONY:debian
debian:
	dpkg-buildpackage -rfakeroot
