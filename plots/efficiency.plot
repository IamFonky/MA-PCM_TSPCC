FILES = system("ls -1v ../data/".ARG1."/")
tostring(i) = "".i
item(i) = word(FILES,i)
path(i) = "../data/".ARG1."/".item(i)
set datafile separator ';'
set style fill transparent solid 0.1 noborder

set xlabel "Number of threads"
set ylabel "Speed [ms]"

set terminal jpeg size 1920,1080 enhanced
set output ARG2.".jpeg"

plot for [i=1:words(FILES)] path(i) using 1:7 smooth mcs with lines lc tostring(i) notitle,\
     for [i=1:words(FILES)] path(i) using 1:7 with points lc tostring(i) lt 1 pt 7 ps 1.5 lw 3 title ARG3.item(i)

