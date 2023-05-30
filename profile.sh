#/bin/bash!

if [ $# -gt 1 ]
then

    cd tspcc

    make clean
    make tspcc

    ./tspcc -f "../maps/$1" -v 64 -s 5 -t 1 -T 16 -i 2 -q 512 -Q 2048 -j 512 -c 10 -o "../data/$2/"

    cd ../plots

    gnuplot -c "plot.plot" "$2" "$3" "Stack size "
    
else
      echo "Script usage : $0 <tsp file> <folder name in data> <plot filename>"

fi