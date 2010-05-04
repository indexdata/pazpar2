#!/bin/bash
PORT=$1
if test -z "$PORT"; then
	PORT=9004
fi


H="http://localhost:${PORT}/search.pz2"
wget -q -O $OF.show.xml "$H?command=exit"
