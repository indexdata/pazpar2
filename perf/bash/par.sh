#!/bin/bash
DELAY=0.1
NUMBER=10
let i=0
while test $i -lt $NUMBER; do
	./client.sh $i >$i.log 2>&1 &
	sleep $DELAY
	let i=$i+1
done
