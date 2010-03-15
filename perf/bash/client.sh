#!/bin/bash
OF=$1
if test -z "$OF"; then
	OF=1
fi
H='http://localhost:9004/search.pz2'
wget -q -O $OF.init.xml "$H/?command=init&service=perf&extra=$OF"
S=`xsltproc get_session.xsl $OF.init.xml`
wget -q -O $OF.search.xml "$H?command=search&query=100&session=$S"
sleep 0.5
wget -q -O $OF.show.xml "$H?command=show&session=$S"
