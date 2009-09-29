#!/bin/sh
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

if test -x ../src/pazpar2; then
    if ../src/pazpar2 -V |grep icu:enabled >/dev/null; then
	exec ${srcdir}/run_pazpar2.sh test_icu
    fi
fi
exit 0
# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
