#!/bin/sh

# srcdir might be set by make
srcdir=${srcdir:-"."}

#TODO set up solr target. For now use donut 
#F=../solr/client.sh 
#
#rm -f solr.pid
#$F -l solr.log -p ztest.pid -D @:9999
#sleep 1
#if test ! -f ztest.pid; then
#    echo "yaz-ztest could not be started"
#    exit 0
#fi

${srcdir}/run_pazpar2.sh test_preferred
E=$?

#kill `cat ztest.pid`
#rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
