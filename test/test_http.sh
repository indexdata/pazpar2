#!/bin/sh
# $Id: test_http.sh,v 1.4 2007-05-16 08:31:17 adam Exp $
#
# Regression test using pazpar2 against z3950.indexdata.com/marc
# Reads Pazpar2 URLs from test_http_urls
#            Outputs to test_http_<no>.log
#            Matches against results in test_http_<no>.res
#


# srcdir might be set by make
srcdir=${srcdir:-"."}

# Fire yp pazpar2
rm -f pazpar2.log
../src/pazpar2 -l pazpar2.log -f ${srcdir}/test_http.cfg -t ${srcdir}/test_http.xml >extra_pazpar2.log 2>&1 &
PP2PID=$!

# Give it a chance to start properly..
sleep 1

# Set to success by default.. Will be set to non-zero in case of failure
code=0

if ps -p $PP2PID >/dev/null 2>&1; then
    :
else
    code=1
    PP2PID=""
    echo "pazpar2 failed to start"
fi

# We can start test for real

oIFS="$IFS"
IFS='
'

testno=1
for f in `cat ${srcdir}/test_http_urls`; do
    if echo $f | grep '^http' >/dev/null; then
	OUT1=${srcdir}/test_http_${testno}.res
	OUT2=test_http_${testno}.log
	DIFF=test_http_${testno}.dif
	if test -f $OUT1; then
	    rm -f $OUT2
	    wget -q -O $OUT2 $f
	    if diff $OUT1 $OUT2 >$DIFF; then
		:
	    else
		echo "Test $testno: Failed. See $OUT1, $OUT2 and $DIFF"
		code=1
	    fi
	else
	    echo "Test $testno: Making for the first time"
	    wget -q -O $OUT1 $f
	    code=1
	fi
	testno=`expr $testno + 1`
    else
	sleep $f
    fi
done
IFS="$oIFS"

sleep 1
# Kill programs

if test -n "$PP2PID"; then
    kill $PP2PID
fi

exit $code

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
