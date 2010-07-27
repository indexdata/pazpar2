#!/bin/sh
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

yaz-ztest -l ztest.log @:9999 & 
ZTEST_PID=$!

# Test using test_http.cfg
${srcdir}/run_pazpar2.sh test_facets

kill $ZTEST_PID

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
