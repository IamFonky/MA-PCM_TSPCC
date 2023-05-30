# MA-PCM

## Projet : Branch & Bound

**Diego Villagrasa**
**Pierre-Benjamin Monaco**

## Usage

If you want to quickly test the code just run `./easyride.sh`. It will build, and execute the program and produce a plot with gnuplot.

If you want to build and run quick tests just type `./profile.sh <map file> <results directory> <plot name>`.

If you want to be more precise with the way you use the program you should compile it

```sh
cd tspcc
make tspcc
```

Then you can use it with the following parameters

```sh
./tspcc
    -v <the verbosity = 0>
    -f <the path to the input file (tsp format) = REQUIRED>
    -o <the path to the output file (csv format) = null>
    -s <the number of samples for each measure = 1>
    -t <the starting number of threads = 2>
    -T <the last number of threads = 2>
    -i <the increment step for threads = 1>
    -q <the starting size of the stack = 10>
    -Q <the size of the last tested stack = 10>
    -j <the increment step for stack size = 1>
    -c <the cutoff depth = 0>
```
