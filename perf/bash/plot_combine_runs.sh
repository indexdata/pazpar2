echo set terminal pdf 
echo set output \"combined_init.pdf\" 
echo "plot '../run_20100511_150215/range.stat' using 1:2 title 'init(marcxml)', '../run_20100511_152140/range.stat' using 1:2 title 'init(turbomarc)' "
echo set output \"combined_search.pdf\"
echo "plot '../run_20100511_150215/range.stat' using 1:3 title 'search(marcxml)', '../run_20100511_152140/range.stat' using 1:3 title 'search(turbomarc)' " 
echo set output \"combined_show.pdf\"
echo "plot '../run_20100511_150215/range.stat' using 1:4 title 'show(marcxml)', '../run_20100511_152140/range.stat' using 1:4 title 'show(turbomarc)' " 
