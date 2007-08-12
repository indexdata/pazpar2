# $Id: Makefile.am,v 1.3 2007-08-12 10:36:33 adam Exp $

check_SCRIPTS = test_http.sh

EXTRA_DIST = marc21.xsl test_http.xml test_http.cfg test_http_urls $(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

dist-hook:
	cp ${srcdir}/test_http_*.res $(distdir)

