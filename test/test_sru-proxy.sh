#!/bin/sh

# srcdir might be set by make
srcdir=${srcdir:-"."}

#TODO set up solr target. For now use donut 
#F=../solr/client.sh 

${srcdir}/run_pazpar2.sh test_sru-proxy
E=$?

#kill `cat ztest.pid`
#rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
