#!/bin/bash
OF=$1
if test -z "$OF"; then
	OF=1
fi

PORT=$2
if test -z "$PORT"; then
	PORT=9004
fi

SERVICE=$3
if test -z "$SERVICE"; then
	SERVICE=perf_t
fi

RECORDS=40
QUERY=100
NUM=20
H="http://localhost:${PORT}/search.pz2"

declare -i MAX_WAIT=2
/usr/bin/time --format "$OF, init, %e" wget -q -O $OF.init.xml "$H/?command=init&service=${SERVICE}&extra=$OF" 2> $OF.init.time
S=`xsltproc get_session.xsl $OF.init.xml`
/usr/bin/time --format "$OF, search, %e" wget -q -O $OF.search.xml "$H?command=search&query=${QUERY}&session=$S" 2> $OF.search.time

let r=0
DO_DISPLAY=true
while [ ${DO_DISPLAY} ] ; do
    SLEEP=$[ ($RANDOM % $MAX_WAIT ) ]
    echo "show in $SLEEP"
    sleep $SLEEP
    /usr/bin/time --format "$OF, show2, %e" wget -q -O $OF.show.$r.xml "$H?command=show&session=$S&start=$r&num=${NUM}&block=1" 2>> $OF.show.time
    AC=`xsltproc get_activeclients.xsl ${OF}.show.$r.xml`
    if [ "$AC" != "0" ] ; then 
	echo "Active clients: ${AC}" 
#    else
#	DO_DISPLAY=false
#	break
    fi
    let r=$r+$NUM
    if [ $r -ge $RECORDS ] ; then 
	DO_DISPLAY=false
	break;
    fi
done
/usr/bin/time --format "$OF, termlist, %e" wget -q -O $OF.termlist.$r.xml "$H?command=termlist&session=$S&name=xtargets%2Csubject%2Cauthor" 2>> $OF.termlist.time
