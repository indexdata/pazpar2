#!/bin/sh

# srcdir might be set by make
srcdir=${srcdir:-"."}

# Using test solr target opencontent-solr
# 
${srcdir}/run_pazpar2.sh test_sort
E=$?

#kill `cat ztest.pid`
#rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
