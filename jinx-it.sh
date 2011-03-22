if [ "$1" == "" ] ; then 
    COUNT=10
else
    COUNT=$1
fi
for d in `seq $COUNT` ; do 
    jinx run src/pazpar2 -X -f ~/etc/pazpar2/server-threaded.xml -l pazpar2.log -v log,warn,fatal
done 