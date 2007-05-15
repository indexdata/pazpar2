# $Id: Makefile.am,v 1.2 2007-05-15 21:40:57 adam Exp $

check_SCRIPTS = test_http.sh

EXTRA_DIST = test_http.xml test_http.cfg test_http_urls $(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

dist-hook:
	cp ${srcdir}/test_http_*.res $(distdir)

