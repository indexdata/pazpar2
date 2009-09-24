#!/bin/sh
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

# Test using test_http.cfg
exec ${srcdir}/run_pazpar2.sh test_post

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
