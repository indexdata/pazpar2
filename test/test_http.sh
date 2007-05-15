#!/bin/sh
# $Id: test_http.sh,v 1.2 2007-05-15 21:28:36 adam Exp $
#
# Regression test using pazpar2 against yaz-ztest
# Reads Pazpar2 URLs from test_http_urls
#            Outputs to test_http_<no>.log
#            Matches against results in test_htttp_<no>.res
#


# srcdir might be set by make
srcdir=${srcdir:-"."}

# Find a suitable yaz-ztest
yt=""
for d in /usr/bin /usr/local/bin ../../yaz/ztest; do
    yt=${d}/yaz-ztest
    if test -x ${yt}; then
	break
    fi
done
if test -z "${yt}"; then
    echo "No yaz-ztest found. Skipping"
    exit 0
fi

# Fire up yaz-ztest (should match port in test_http.xml)
$yt -l test_http_ztest.log tcp:@:9764 &
YTPID=$!

# Fire yp pazpar2
rm -f pazpar2.log
../src/pazpar2 -l pazpar2.log -f ${srcdir}/test_http.cfg -t ${srcdir}/test_http.xml >extra_pazpar2.log 2>&1 &
PP2PID=$!

# Give both programs room to start properly..
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

if ps -p $YTPID >/dev/null 2>&1; then
    :
else
    code=1
    YTPID=""
    echo "yaz-ztest failed to start"
fi
# We can start test for real

testno=1
for f in `cat ${srcdir}/test_http_urls`; do
    OUT1=${srcdir}/test_http_${testno}.res
    OUT2=${srcdir}/test_http_${testno}.log
    DIFF=${srcdir}/test_http_${testno}.dif
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
done

sleep 1
# Kill programs
if test -n "$YTPID"; then
    kill $YTPID
fi

if test -n "$PP2PID"; then
    kill $PP2PID
fi

exit $code

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
