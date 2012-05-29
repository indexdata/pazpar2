#!/bin/bash
NUMBER=$1
if [ -z "$NUMBER" ] ; then
    NUMBER=20
fi

MAX_DELAY=1
CLIENT_WAIT=2
ROUNDS=10
let r=0
PORT=$2
SERVICE=$3
CLIENT_SCRIPT="client_timed.sh"
rm -f *.time
if [ "$TMP_DIR" == "" ] ; then  
    export TMP_DIR=run_`date +"%Y%m%d_%H%M%S"`/
fi  
mkdir -p ${TMP_DIR} 
rm -f latest
ln -s ${TMP_DIR} latest
while test $r -lt $ROUNDS; do
    echo "$r"
    i=0
    while test $i -lt $NUMBER; do
	./${CLIENT_SCRIPT} $r.$i $PORT $SERVICE >$r.$i.log 2>&1 &
	SLEEP=$[ ( $RANDOM % $MAX_DELAY ) ]
	sleep $SLEEP
	CLIENTS=`ps -ef |grep ${CLIENT_SCRIPT} | grep -cv grep` 
	while test $CLIENTS -ge $NUMBER ; do
	    sleep $CLIENT_WAIT
	    CLIENTS=`ps -ef |grep ${CLIENT_SCRIPT} |grep -cv grep ` 
	    echo "Active $CLIENTS"
	done
	let i=$i+1
    done
    let r=$r+1
done
wait
cat ${TMP_DIR}*.time >> ${TMP_DIR}timed.$SERVICE.$NUMBER.log
#wget --tries=1 -O x "http://localhost:${PORT}/?command=exit"

