#!/bin/sh
res=0
for m in tmarc?.xml; do
	b=`basename $m .xml`
	l=$b.log.xml
	r=$b.res.xml
	d=$b.dif
	xsltproc ../etc/tmarc.xsl $m >$l
	if test -f $r; then
		if diff $l $r >$d; then
			rm $d
		else
			echo "$b: FAIL; check $d"
			res=1
		fi
	else
		echo "$b: making $r for the first time"
		mv $l $r
		res=1
	fi
done
exit $res
