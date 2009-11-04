#!/bin/sh
#
# Regression test using pazpar2 against z3950.indexdata.com/marc or gils
# Reads Pazpar2 URLs from $1
#            Outputs to $1_<no>.log
#            Matches against results in $1_<no>.res
# Requires curl

# srcdir might be set by make
srcdir=${srcdir:-"."}

# look for curl in PATH
oIFS=$IFS
IFS=:
curl=''
for p in $PATH; do
    if test -x $p/curl; then
	curl=$p/curl
	break
    fi
done
IFS=$oIFS

if test -z $curl; then
    echo "curl not found. $PREFIX can not be tested"
    exit 1
fi
GET='$curl --silent --output $OUT2 "$f"'
POST='$curl --silent --header "Content-Type: text/xml" --data-binary "@$postfile" --output $OUT2  "$f"'

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

if test -n "$PAZPAR2_USE_VALGRIND"; then
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
	if test ! -f $OUT2; then
	    touch $OUT2
	fi
	if test -f $OUT1; then
	    if diff $OUT1 $OUT2 >$DIFF; then
		:
	    else
		echo "Test $testno: Failed. See $OUT1, $OUT2 and $DIFF"
		echo "URL: $f"
		code=1
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
	IFS="$oIFS"
	echo "Test $testno: pazpar2 died"
	exit 1
    fi
done

# Kill programs

if test -n "$PP2PID"; then
    kill $PP2PID
    sleep 2
fi

exit $code

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
