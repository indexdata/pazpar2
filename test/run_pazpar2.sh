#!/bin/sh
#
# Regression test using pazpar2 against z3950.indexdata.com/marc or gils
# Reads Pazpar2 URLs from $1
#            Outputs to $1_<no>.log
#            Matches against results in $1_<no>.res
# Requires curl

# srcdir might be set by make
srcdir=${srcdir:-"."}

# terminate pazpar2 if test takes more than this (in seconds)
WAIT=120

kill_pazpar2()
{
    if test -n "$PP2PID"; then
	kill $PP2PID
    fi
    if test -n "$SLEEP_PID"; then
	kill $SLEEP_PID
	SLEEP_PID=""
    fi
    if test -f ztest.pid; then
	kill `cat ztest.pid`
	rm -f ztest.pid
    fi
}

ztest=false
icu=false
while test $# -gt 0; do
    case "$1" in
        -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
        *) optarg= ;;
    esac
    case $1 in
        --ztest)
            ztest=true
            ;;
        --icu)
            icu=true
            ;;
        -*)
	    echo "Bad option $1"
	    exit 1
            ;;
	*)
	    PREFIX=$1
	    ;;
    esac
    shift
done

if test "x${PREFIX}" = "x"; then
    echo Missing prefix for run_pazpar2.sh
    exit 1
fi

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
    echo "Test $PREFIX: curl not found"
    exit 1
fi

if test "$icu" = "true"; then
    if ../src/pazpar2 -V |grep icu:enabled >/dev/null; then
	:
    else
	echo "Skipping test ${PREFIX}: ICU support unavailable"
	exit 0
    fi
fi

if test "$ztest" = "true" ; then
    oIFS=$IFS
    IFS=:
    F=''
    for p in $PATH; do
	if test -x $p/yaz-ztest -a -x $p/yaz-client; then
	    VERSION=`$p/yaz-client -V|awk '{print $3;}'|awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
            if test $VERSION -ge 4002052; then
		F=$p/yaz-ztest
		break
            fi
	fi
    done
    IFS=$oIFS
    if test -z "$F"; then
	echo "Skipping test ${PREFIX}: recent yaz-ztest not found"
	exit 0
    fi
    rm -f ztest.pid
    rm -f ${PREFIX}_ztest.log
    $F -l ${PREFIX}_ztest.log -p ztest.pid -D tcp:localhost:9999
    sleep 1
    if test ! -f ztest.pid; then
	echo "yaz-ztest could not be started"
	exit 0
    fi
fi

# remove log if starting pazpar2
if [ -z "$SKIP_PAZPAR2" ] ; then
    rm -f ${PREFIX}_pazpar2.log
fi

CFG=${PREFIX}.cfg
URLS=${PREFIX}.urls
VALGRINDLOG=${PREFIX}_valgrind.log

if test `uname` = "Linux"; then
    sec=0.3
    maxrounds=20
else
    sec=1
    maxrounds=10
fi
LEVELS=loglevel,fatal,warn,log,debug,notime,zoom,zoomdetails
if test -n "$PAZPAR2_USE_VALGRIND"; then
    valgrind --num-callers=30 --show-reachable=yes --leak-check=full --log-file=$VALGRINDLOG ../src/pazpar2 -v $LEVELS -X -l ${PREFIX}_pazpar2.log -f ${CFG} >${PREFIX}_extra_pazpar2.log 2>&1 &
    PP2PID=$!
    sleep 6
elif test -n "$SKIP_PAZPAR2"; then
    echo "Test ${PREFIX}: not starting Pazpar2 (should be running already)"
else
    ../src/pazpar2 -v $LEVELS -d -X -l ${PREFIX}_pazpar2.log -f ${srcdir}/${CFG} >${PREFIX}_extra_pazpar2.log 2>&1 &
    PP2PID=$!
    sleep 2
fi

if [ -z "$SKIP_PAZPAR2" -a -z "$WAIT_PAZPAR2" ] ; then
    if ps -p $PP2PID >/dev/null 2>&1; then
	(sleep $WAIT; kill_pazpar2 >/dev/null) &
	SLEEP_PID=$!
	trap kill_pazpar2 INT
	trap kill_pazpar2 HUP
    else
	echo "Test ${PREFIX}: pazpar2 failed to start"
	if test -f ztest.pid; then
	    kill `cat ztest.pid`
	    rm -f ztest.pid
	fi
	exit 1
    fi
fi

GET='$curl --silent --output $OUT2 "$f"'
POST='$curl --silent --header "Content-Type: text/xml" --data-binary "@$postfile" --output $OUT2  "$f"'

# Set to success by default.. Will be set to non-zero in case of failure
code=0

# We can start test for real
testno=1
rounds=1
for f in `cat ${srcdir}/${URLS}`; do
    if echo $f | grep '^http' >/dev/null; then
	OUT1=${srcdir}/${PREFIX}_${testno}.res
	OUT2=${PREFIX}_${testno}.log
	DIFF=${PREFIX}_${testno}.dif
	rm -f $OUT2 $DIFF
	if [ -n "$DEBUG" ] ; then
	    echo "test $testno: $f"
	fi
	while test $rounds -gt 0; do
	    if test -n "${postfile}"; then
		eval $POST
	    else
		eval $GET
	    fi
	    if test ! -f $OUT2; then
		touch $OUT2
	    fi
	    rounds=`expr $rounds - 1`
	    if test -f $OUT1 -a -z "$PAZPAR2_OVERRIDE_TEST"; then
		if diff $OUT1 $OUT2 >$DIFF; then
		    rm $DIFF
		    rm $OUT2
		    rounds=0
		else
		    if test $rounds -eq 0; then
			echo "Test $testno: Failed. See $OUT1, $OUT2 and $DIFF"
			echo "URL: $f"
			code=1
		    fi
		fi
	    else
		if test $rounds -eq 0; then
		    echo "Test $testno: Making for the first time"
		    mv $OUT2 $OUT1
		    code=1
		fi
	    fi
	    if test $rounds -gt 0; then
		sleep $sec
	    fi
	done
	testno=`expr $testno + 1`
	postfile=
	rounds=1
    elif echo $f | grep '^[0-9]' >/dev/null; then
	rounds=$maxrounds
    else
	if test -f $srcdir/$f; then
	    postfile=$srcdir/$f
	else
	    echo "File $f does not exist"
	    code=1
	fi
    fi
    if [ -z "$SKIP_PAZPAR2" ] ; then
	if ps -p $PP2PID >/dev/null 2>&1; then
	    :
	else
	    IFS="$oIFS"
	    if test -n "$SLEEP_PID"; then
		echo "Test $testno: pazpar2 terminated (timeout, probably)"
	    else
		echo "Test $testno: pazpar2 died"
	    fi
	    exit 1
	fi
    fi
done

if [ "$WAIT_PAZPAR2" ] ; then
    i=0
    while test $i -lt $WAIT_PAZPAR2; do
	i=`expr $i + 1`
	echo -n "$i."
	sleep 60
    done
    echo "done"
fi
# Kill programs
if test -f ztest.pid; then
    kill `cat ztest.pid`
    rm -f ztest.pid
fi

if [ -z "$SKIP_PAZPAR2" ] ; then
    kill_pazpar2
    sleep 2
fi
exit $code

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
