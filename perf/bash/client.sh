#!/bin/bash
O=$1
if test -z "$O"; then
	O=1
fi
H='http://localhost:9004/search.pz2'
wget -q -O $O.init.xml "$H/?command=init"
S=`xsltproc get_session.xsl $O.init.xml`
wget -q -O $O.search.xml "$H?command=search&query=utah&session=$S"
sleep 0.5
wget -q -O $O.show.xml "$H?command=show&session=$S"
