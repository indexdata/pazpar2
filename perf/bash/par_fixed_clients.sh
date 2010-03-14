#!/bin/bash
DELAY=0.001
WAIT=1
NUMBER=100
ROUNDS=5
let r=0
while test $r -lt $ROUNDS; do
	echo "$r"
	i=0
	while test $i -lt $NUMBER; do
		./client.sh $r.$i >$r.$i.log 2>&1 &
		CLIENTS=`ps -ef |grep -c client.sh` 
		while test $CLIENTS -ge $NUMBER ; do
		    sleep $WAIT
		    CLIENTS=`ps -ef |grep -c client.sh` 
		    echo "Active $CLIENTS"
		done
		let i=$i+1
	done
	let r=$r+1
done
wait
wget -O x 'http://localhost:8010/?command=exit'
