# This file is part of Pazpar2.
check_SCRIPTS = test_http.sh test_icu.sh

EXTRA_DIST = run_pazpar2.sh marc21.xsl z3950_indexdata_com_marc.xml \
	z3950_indexdata_com_gils.xml test_http.cfg gils_service.xml \
	test_http_urls test_icu.cfg test_icu_urls $(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

dist-hook:
	cp ${srcdir}/test_http_*.res $(distdir)
	cp ${srcdir}/test_icu_*.res $(distdir)

