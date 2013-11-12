#!/bin/sh

TEST=`basename $0 .sh`
# srcdir might be set by make
srcdir=${srcdir:-"."}

exec ${srcdir}/run_pazpar2.sh --icu --ztest $TEST

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
