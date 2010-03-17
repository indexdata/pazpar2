
FILE=timed.$1.log
TEMP=${FILE/timed./}
USERS=${TEMP/.log/}
#echo $USERS $FILE

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

INIT=`stat_word "init" `
SEARCH=`stat_word "search"`
SHOW=`stat_word "show"`
echo "$USERS $INIT $SEARCH $SHOW" 
