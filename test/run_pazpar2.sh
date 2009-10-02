#!/bin/sh
#
# Regression test using pazpar2 against z3950.indexdata.com/marc
# Reads Pazpar2 URLs from test_http_urls
#            Outputs to test_http_<no>.log
#            Matches against results in test_http_<no>.res
#


# srcdir might be set by make
srcdir=${srcdir:-"."}

if test -x /usr/bin/curl; then
    GET='/usr/bin/curl -s -o $OUT2 "$f"'
    POST='/usr/bin/curl -s -H "Content-Type: text/xml" --data-binary "@$postfile" -o $OUT2  "$f"'
elif test -x /usr/bin/wget; then
    GET='/usr/bin/wget -q -O $OUT2 $f'
    POST='/usr/bin/wget -q -O $OUT2 --header="Content-Type: text/xml" --post-file=$postfile $f'
elif test -x /usr/bin/lynx; then
    GET='/usr/bin/lynx -dump "$f" >$OUT2'
    POST=''
fi

# Fire up pazpar2
rm -f pazpar2.log


PREFIX=$1
if test "x${PREFIX}" = "x"; then
    echo Missing prefix for run_pazpar2.sh
    exit 1
fi
CFG=${PREFIX}.cfg
URLS=${PREFIX}_urls
VALGRINDLOG=${PREFIX}_valgrind.log

usevalgrind=false
if $usevalgrind; then
    valgrind --leak-check=full --log-file=$VALGRINDLOG ../src/pazpar2 -X -l pazpar2.log -f ${CFG} >extra_pazpar2.log 2>&1 &
else
    YAZ_LOG=zoom,zoomdetails,debug,log,fatal ../src/pazpar2 -d -X -l pazpar2.log -f ${srcdir}/${CFG} >extra_pazpar2.log 2>&1 &
fi


PP2PID=$!

# Give it a chance to start properly..
sleep 3

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
for f in `cat ${srcdir}/${URLS}`; do
    if echo $f | grep '^http' >/dev/null; then
	OUT1=${srcdir}/${PREFIX}_${testno}.res
	OUT2=${PREFIX}_${testno}.log
	DIFF=${PREFIX}_${testno}.dif
	rm -f $OUT2 $DIFF
	if test -n "${postfile}"; then
	    eval $POST
	else
	    eval $GET
	fi
	if test -f $OUT1; then
	    if test -f $OUT2; then
		if diff $OUT1 $OUT2 >$DIFF; then
		    :
		else
		    # wget returns 0-size file on HTTP error, curl dont.
		    if test -s $OUT1; then
			echo "Test $testno: Failed. See $OUT1, $OUT2 and $DIFF"
			echo "URL: $f"
			code=1
		    fi
		fi
	    else
		echo "Test $test: can not be performed"
	    fi
	else
	    echo "Test $testno: Making for the first time"
	    mv $OUT2 $OUT1
	    code=1
	fi
	testno=`expr $testno + 1`
	postfile=
    elif echo $f | grep '^[0-9]' >/dev/null; then
	sleep $f
    else
	if test -f $f; then
	    postfile=$f
	else
	    echo "File $f does not exist"
	    code=1
	fi
    fi
    if ps -p $PP2PID >/dev/null 2>&1; then
	:
    else
	echo "Test $testno: pazpar2 died"
	exit 1
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
