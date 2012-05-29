COUNT=20
if [ "$1" != "" ] ; then
    COUNT=$1
fi
PORT=9004
if [ "$2" != "" ] ; then
    PORT=$2
fi
# If not specifying SERVICE, using default
if [ "$3" != "" ] ; then
    SERVICE=$3
fi
while true ; do 
    export TMP_DIR=pid_$$/run_`date +"%Y%m%d_%H%M%S"`/
    sh par_fixed_clients.sh $COUNT $PORT $SERVICE ; 
done 
