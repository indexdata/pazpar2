#!/bin/sh
# $Id: test_http.sh,v 1.1 2007-05-15 08:56:03 adam Exp $
srcdir=${srcdir:-"."}
./pazpar2 -f ${srcdir}/test_http.cfg -t ${srcdir}/test_http.xml >test_http.log 2>&1 &
PP2PID=$!
sleep 1
if ps -p $PP2PID >/dev/null 2>&1; then
    :
    # echo "Started OK PID=$PP2PID"
else
    echo "pazpar2 failed to start"
    exit 1
fi

kill $PP2PID
exit 0

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
