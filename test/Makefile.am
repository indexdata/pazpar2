# This file is part of Pazpar2.
check_SCRIPTS = test_http.sh test_icu.sh test_post.sh test_settings.sh test_turbomarcxml.sh test_facets.sh

EXTRA_DIST = run_pazpar2.sh marc21_test.xsl tmarc.xsl \
	z3950_indexdata_com_marc.xml \
	gils_service.xml marc_service.xml marc21.mmap \
	test_http.cfg test_http_urls \
	test_icu.cfg test_icu_urls \
	test_post.cfg test_post_urls \
	test_settings.cfg test_settings_urls \
	test_turbomarcxml.cfg test_turbomarcxml_urls \
	test_facets.cfg test_facets_urls \
	$(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

dist-hook:
	cp ${srcdir}/test_*.res $(distdir)
	mkdir $(distdir)/set2
	cp ${srcdir}/set2/*.xml $(distdir)/set2
