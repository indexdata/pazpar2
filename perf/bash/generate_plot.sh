
if [ "$1" != "" ] ; then 
    echo "set terminal $1" > range.gnuplot
    echo "set terminal $1" > init.gnuplot
    echo "set terminal $1" > search.gnuplot
    echo "set terminal $1" > show.gnuplot
else 
    echo "" > range.gnuplot
    echo "" > init.gnuplot
    echo "" > search.gnuplot
    echo "" > show.gnuplot
fi

echo "plot 'range.stat' using 1:2 title 'init', 'range.stat'using 1:3 title 'search', 'range.stat'using 1:4 title 'show'" >> range.gnuplot
echo "plot 'init.stat' using 1:2 title 'init'" >> init.gnuplot
echo "plot 'search.stat' using 1:2 title 'Search'" >> search.gnuplot
echo "plot 'search.stat' using 1:2 title 'Search'" >> show.gnuplot

if [ "$1" != "" ] ; then 
    gnuplot < range.gnuplot > range.$1
    gnuplot < init.gnuplot > init.$1
    gnuplot < search.gnuplot > search.$1
    gnuplot < show.gnuplot > show.$1
else 
    gnuplot < range.gnuplot 
    gnuplot < init.gnuplot 
    gnuplot < search.gnuplot
    gnuplot < show.gnuplot
fi 

cat range.gnuplot init.gnuplot search.gnuplot show.gnuplot > all.gnuplot
