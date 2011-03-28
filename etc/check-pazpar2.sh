#!/bin/sh

usage()
{
cat <<EOF    
usage: $0 options

This script run the test1 or test2 over a machine.

OPTIONS:
   -h      Show this message
   -H      host name or IP
   -w      Warning level
   -c      Critical level
   -p      port name
EOF
}


MSG="OK";
HOST=localhost
PORT=8004

while getopts "hH:w:c:p:" OPTION
do 
    case $OPTION in
     h)  usage; exit 3;;
     H)  
	    HOST=$OPTARG
	    ;;
     p)  
	    PORT=$OPTARG
	    ;;
     w)  
	    WARN_LEVEL=$OPTARG
	    ;;
     c)  
	    CRIT_LEVEL=$OPTARG
	    ;;
  esac
done

#echo $HOST $PORT $WARN_LEVEL $CRIT_LEVEL

`wget -q "http://$HOST:$PORT/search.pz2?command=server-status" -O- | xsltproc /etc/pazpar2/server-status-nagios.xsl - 2> /dev/null` 

if [ $? -ne 0 ] ; then
    MSG="FATAL failed to communicate with pazpar2"
    exit 128
fi

if [ "$WARN_LEVEL" != "" ] ; then 
    if [ $SESSIONS -gt $WARN_LEVEL ];  then
	MSG="WARNING "
	rc=1; 
    fi
fi 

if [ "$CRIT_LEVEL" != "" ] ; then 
    if [ $SESSIONS -gt $CRIT_LEVEL ];  then
	MSG="CRITICAL " 
	rc=2;
    fi
fi 

echo "SESSIONS $MSG $SESSIONS ($CLIENTS) [$VIRT,$VIRTUSE,$AREA,$ORDBLKS,$UORDBLKS,$FORDBLKS,$KEEPCOST,$HBLKS,$HBLKHD]   | $SESSIONS;$WARN_LEVEL;$CRIT_LEVEL "
exit $rc

