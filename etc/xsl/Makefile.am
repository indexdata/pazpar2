TESTS = $(check_SCRIPTS)

check_SCRIPTS = test_xsl.sh

dist-hook:
	rm -f ${srcdir}/*.log.xml
	cp ${srcdir}/*.xml $(distdir)
	cp ${srcdir}/*.xsl $(distdir)

