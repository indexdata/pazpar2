#!/bin/sh

# srcdir might be set by make
srcdir=${srcdir:-"."}

# look for yaz-ztest in PATH
oIFS=$IFS
IFS=:
F=''
for p in $PATH; do
    if test -x $p/yaz-ztest -a $p/yaz-config; then
	VERSION=`$p/yaz-config -V|awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
	if test $VERSION -ge 4000012; then
	    F=$p/yaz-ztest
            break
	fi
    fi
done
IFS=$oIFS

if test -z "$F"; then
   echo "yaz-ztest not found that supports facets"
   exit 0
fi

rm -f ztest.pid
$F -l ztest.log -p ztest.pid -D @:9999
sleep 1
if test ! -f ztest.pid; then
    echo "yaz-ztest could not be started"
    exit 0
fi

${srcdir}/run_pazpar2.sh test_facets
E=$?

kill `cat ztest.pid`
rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
