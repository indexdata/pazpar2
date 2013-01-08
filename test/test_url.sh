#!/bin/sh
#

TEST=`basename $0 .sh`
# srcdir might be set by make
srcdir=${srcdir:-"."}

if test -x ../src/pazpar2; then
    if ../src/pazpar2 -V |grep icu:enabled >/dev/null; then
	exec ${srcdir}/run_pazpar2.sh $TEST
    fi
fi

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
