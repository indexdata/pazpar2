
r=$1
MAX=$2
PORT=$3
SERVICE=$4
RANGE=range
rm -f init.stat search.stat show.stat $RANGE.stat
while test $r -le $MAX ; do
    sh par_fixed_clients.sh $r $PORT $SERVICE
    sh stat_file.sh $r $SERVICE >> $RANGE.stat
    let r=$r+1
done

DIR=run_`date +"%Y%m%d_%H%M%S"`
mkdir $DIR
mv *.stat *.log *.time $DIR

#mv init.stat $PID.init.stat
#mv search.stat $PID.search.stat
#mv show.stat $PID.show.stat
#mv $RANGE.stat $PID.show.stat
