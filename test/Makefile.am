# $Id: Makefile.am,v 1.1 2007-05-15 15:50:48 adam Exp $

check_SCRIPTS = test_http.sh

EXTRA_DIST = test_http.xml test_http.cfg test_http_urls $(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log

dist-hook:
	cp test_http_*.res $(distdir)

