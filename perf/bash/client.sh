#!/bin/bash
OF=$1
if test -z "$OF"; then
	OF=1
fi

PORT=$2
if test -z "$PORT"; then
	PORT=9004
fi


H="http://localhost:${PORT}/search.pz2"
wget -q -O $OF.init.xml "$H/?command=init&service=perf&extra=$OF"
S=`xsltproc get_session.xsl $OF.init.xml`
wget -q -O $OF.search.xml "$H?command=search&query=100&session=$S"
sleep 1
wget -q -O $OF.show.xml "$H?command=show&session=$S&num=100&block=1"
