#!/bin/sh
# $Id: test_http.sh,v 1.11 2007-09-10 08:18:19 adam Exp $
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

# Test using test_http.cfg
exec ${srcdir}/run_pazpar2.sh test_http

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
