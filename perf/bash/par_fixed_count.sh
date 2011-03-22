
if [ "$2" == "" ] ; then 
    echo "Missing range" 
    exit 1
fi

LOOP=$3
if [ "$3" == "" ] ; then 
    LOOP=1
fi

for e in `seq $LOOP` ; do 
    for d in `seq $1 $2` ; do 
	sh ./par_fixed_clients.sh $d 9005 perf_t ; sleep 120 ; 
    done
done
