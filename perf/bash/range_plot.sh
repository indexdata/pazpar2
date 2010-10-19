
if [ "$2" != "" ] ; then
  echo  "set terminal $2" 
fi

if [ "$1" != "" ] ; then 
    RANGE=$1
else
    RANGE=range
fi

echo "plot '$RANGE.stat' using 1:2 title 'init', '$RANGE.stat'using 1:3 title 'search', '$RANGE.stat'using 1:4 title 'show' " 
