#!/bin/bash
NUMBER=$1
if [ -z "$NUMBER" ] ; then
    NUMBER=20
fi

DELAY=0.001
WAIT=1
#NUMBER=100
ROUNDS=2
let r=0
PORT=9005
CLIENT_SCRIPT="client_timed.sh"
while test $r -lt $ROUNDS; do
	echo "$r"
	i=0
	while test $i -lt $NUMBER; do
		./${CLIENT_SCRIPT} $r.$i $PORT >$r.$i.log 2>&1 &
		CLIENTS=`ps -ef |grep -c ${CLIENT_SCRIPT}` 
		while test $CLIENTS -ge $NUMBER ; do
		    sleep $WAIT
		    CLIENTS=`ps -ef |grep -c ${CLIENT_SCRIPT}` 
		    echo "Active $CLIENTS"
		done
		let i=$i+1
	done
	let r=$r+1
done
wait
cat *.time > timed.$NUMBER.log
#wget --tries=1 -O x "http://localhost:${PORT}/?command=exit"
sleep 5