#!/bin/bash
OF=$1
if test -z "$OF"; then
	OF=1
fi

PORT=$2
if test -z "$PORT"; then
	PORT=9004
fi

QUERY=100
SERVICE=perf_t

H="http://localhost:${PORT}/search.pz2"

/usr/bin/time --format "$OF, init, %e" wget -q -O $OF.init.xml "$H/?command=init&service=${SERVICE}&extra=$OF" 2> $OF.init.time
S=`xsltproc get_session.xsl $OF.init.xml`
/usr/bin/time --format "$OF, search, %e" wget -q -O $OF.search.xml "$H?command=search&query=${QUERY}&session=$S" 2> $OF.search.time
sleep 1
# First show
/usr/bin/time --format "$OF, show, %e" wget -q -O $OF.show.xml "$H?command=show&session=$S" 2> $OF.show.time
AC=`xsltproc get_activeclients.xsl ${OF}.show.xml`
echo "Active clients: $AC " 
if [ "${AC}" != "0" ] ; then
    echo "Active clients: ${AC}" 
    /usr/bin/time --format "$OF, show2, %e" wget -q -O $OF.show.xml "$H?command=show&session=$S" 2>> $OF.show.time
    AC=`xsltproc get_session.xsl $OF.show.xml`
fi
