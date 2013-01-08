#!/bin/sh

# srcdir might be set by make
srcdir=${srcdir:-"."}

TEST=`basename $0 .sh`

# Using test solr target opencontent-solr
E=0
if test -x ../src/pazpar2; then
    if ../src/pazpar2 -V |grep icu:enabled >/dev/null; then
	${srcdir}/run_pazpar2.sh $TEST
	E=$?
    fi
fi

#kill `cat ztest.pid`
#rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
