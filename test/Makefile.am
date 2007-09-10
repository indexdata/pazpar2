# $Id: Makefile.am,v 1.5 2007-09-10 16:25:51 adam Exp $

check_SCRIPTS = test_http.sh test_icu.sh

EXTRA_DIST = run_pazpar2.sh marc21.xsl test_http.xml test_http.cfg \
	test_http_urls test_icu_urls $(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

dist-hook:
	cp ${srcdir}/test_http_*.res $(distdir)
	cp ${srcdir}/test_url_*.res $(distdir)

