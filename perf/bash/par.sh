#!/bin/bash
DELAY=0.001
WAIT=5
NUMBER=10
ROUNDS=5
if test -n "$1"; then
	. $1
fi
let r=0
while test $r -lt $ROUNDS; do
	echo "$r"
	let i=0
	while test $i -lt $NUMBER; do
		./client.sh $r.$i >$r.$i.log 2>&1 &
		sleep $DELAY
		let i=$i+1
	done
	sleep $WAIT
	let r=$r+1
done
wait
wget -O x 'http://localhost:9004/?command=exit'
