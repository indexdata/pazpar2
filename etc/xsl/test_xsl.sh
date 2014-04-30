#!/bin/sh
res=0
for x in *.xsl; do
    t=`basename $x .xsl`
    if test ! -f ${t}1.xml; then
	continue
    fi
    echo "$t"
    for m in ${t}?.xml; do
	b=`basename $m .xml`
	opt=`grep 'xsltproc:' $m|cut -d: -f2|cut -d- -f1|tr '#' '-'`
	l=$b.log.xml
	r=$b.res.xml
	d=$b.dif
	xsltproc $opt $x $m >$l
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
done
exit $res
