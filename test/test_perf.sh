#!/bin/sh

TEST=`basename $0 .sh`
# srcdir might be set by make
srcdir=${srcdir:-"."}

if test -z "$PERF_PROG"; then
    if test -x /usr/bin/time; then
        PERF_PROG="/usr/bin/time -p"
    fi
fi
export PERF_PROG
exec ${srcdir}/run_pazpar2.sh --ztest --icu $TEST

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
