#!/bin/sh
# $Id: test_icu.sh,v 1.1 2007-09-10 16:25:51 adam Exp $
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

if test -x ../src/pazpar2; then
    if ../src/pazpar2 -V |grep icu: >/dev/null; then
	exec ${srcdir}/run_pazpar2.sh test_icu
    fi
fi
exit 0
# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
