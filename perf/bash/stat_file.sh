

stat_word () {
    WORD=$1
    NUMBERS=`grep $WORD $FILE | cut -d , -f 3`
#    echo NUMBERS $NUMBERS
    SUM=`(for d in $NUMBERS ; do echo " $d + " ; done  ; echo "0" ) `
    SUM=`echo $SUM | bc`
#    echo SUM $SUM
    COUNT=`(for d in $NUMBERS ; do  echo " 1 + " ; done  ; echo "0")`
    COUNT=`echo $COUNT | bc`
#    echo COUNT $COUNT
    AVG=`echo "scale=3; $SUM / ($COUNT) " | bc`
    echo "$AVG"
}

SERVICE=perf_t
if [ "$2" != "" ] ; then 
    SERVICE=$2
fi
FILE=timed.$SERVICE.$1.log
USERS=$1
if [ -f $FILE ] ; then
    INIT=`stat_word "init" `
    grep init $FILE   | sed -e "s/^.*,/$USERS /" >> init.stat
    SEARCH=`stat_word "search"`
    grep search $FILE | sed -e "s/^.*,/$USERS /" >> search.stat
    SHOW=`stat_word "show"`
    grep show $FILE   | sed -e "s/^.*,/$USERS /" >> show.stat

    echo "$1 $INIT $SEARCH $SHOW" 
else
    echo "# no such file $FILE" 
fi

