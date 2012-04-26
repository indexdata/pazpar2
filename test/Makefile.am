# This file is part of Pazpar2.
check_SCRIPTS = test_http.sh test_icu.sh test_post.sh \
 test_settings.sh test_turbomarcxml.sh test_facets.sh \
 test_solr.sh test_url.sh test_filter.sh \
 test_termlist_block.sh test_limit_limitmap.sh

EXTRA_DIST = run_pazpar2.sh marc21_test.xsl tmarc.xsl solr-pz2.xsl \
	z3950_indexdata_com_marc.xml \
	gils_service.xml marc_service.xml marc21.mmap \
	test_facets.cfg test_facets.urls \
	test_icu.cfg test_icu.urls \
	test_http.cfg test_http.urls \
	test_post.cfg test_post.urls \
	test_settings.cfg test_settings.urls \
	test_solr.cfg test_solr.urls \
	test_solr_settings_1.xml  test_solr_settings_2.xml  test_solr_settings_3.xml  test_solr_settings_4.xml \
	test_turbomarcxml.cfg test_turbomarcxml.urls \
	test_url.cfg test_url.urls \
	test_filter.cfg test_filter.urls \
	test_termlist_block.cfg test_termlist_block.urls \
	test_facets_settings_1.xml  test_facets_settings_2.xml \
	test_url_service.xml  test_url_settings_1.xml  test_url_settings_2.xml  test_url_settings_3.xml  test_url_settings_4.xml \
	test_limit_limitmap.cfg test_limit_limitmap.urls \
	test_limit_limitmap_service.xml \
	test_limit_limitmap_setting_1.xml test_limit_limitmap_setting_2.xml \
	test_limit_limitmap_setting_3.xml test_limit_limitmap_setting_4.xml \
	test_limit_limitmap_setting_5.xml \
	ztest_localhost_settings.xml test_termlist_block_settings_2.xml \
	$(check_SCRIPTS)

TESTS = $(check_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

CONFIG_CLEAN_FILES=*.log *.dif

clean-local:
	rm -f *.log *.dif

dist-hook:
	cp ${srcdir}/test_*.res $(distdir)
	mkdir $(distdir)/set2
	cp ${srcdir}/set2/*.xml $(distdir)/set2
