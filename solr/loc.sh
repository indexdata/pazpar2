#!/bin/bash
LOG=index.log
MARCDUMP="yaz-marcdump"

if [ "$SOLR_URL" == "" ] ; then 
    DEF_HOST=-Durl="http://localhost:8983/solr/update" 
else
    DEF_HOST=-Durl="$SOLR_URL"
fi

if [ -d "./data" ] ; then
        LOCDATA="./data"
else
        LOCDATA=/extra/heikki/locdata
fi

if [ ! -d "$LOCDATA" ] ; then
	echo "$LOCDATA not a directory"
	exit 1
fi

if [ "$1" == "" ] ; then 
    FILES="$LOCDATA/part*"
else 
    FILES="$*"
fi 
#echo $FILES

rm -f $LOG

function convert()
{
    FILE=$2
    echo "zcat $1 > $FILE.mrc" 
    zcat $1 > $FILE.mrc
    $MARCDUMP  -f marc8 -t utf-8 -o turbomarc $FILE.mrc > $FILE.xml
    xsltproc ../test/tmarc.xsl $FILE.xml  > $FILE.pz 
    xsltproc ../etc/pz2-solr.xsl $FILE.pz > $FILE.solr
    ls -l $FILE.* >> $LOG
}

if [ "$TWO_PASS" == "1" ] ; then 
    for d in ${FILES} ; do
	date  "+%c converting $d" >>$LOG
	FILE=`basename $1`
	convert $d $FILE
    done
fi

for d in ${FILES} ; do
	date  "+%c converting $d" >>$LOG
	BASE=`basename $d`
	FILE=$BASE.solr
	if [ ! -f "$FILE" ] ; then
	    convert $d $BASE
	fi
	date  "+%c indexing $d" >>$LOG
	java $DEF_HOST -jar post.jar $FILE
	date  "+%c indexing $d ended" >>$LOG
	#rm tmp.*
done
date  "+%c All done" >>$LOG
exit 0
