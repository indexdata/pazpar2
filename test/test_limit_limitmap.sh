#!/bin/bash
#

FILE=`basename $0`
TEST=${FILE/.sh/}
# srcdir might be set by make
srcdir=${srcdir:-"."}

# Test using test_http.cfg
exec ${srcdir}/run_pazpar2.sh $TEST

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
