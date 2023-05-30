#  Copyright (c) 2012 Marcelo Pasin. All rights reserved.

CFLAGS=-O3 -Wall
LDFLAGS=-O3 -lm

all: tspcc

tspcc: tspcc.o
	c++ -o tspcc $(LDFLAGS) tspcc.o
	
tspcc.o: tspcc.cpp graph.hpp path.hpp tspfile.hpp lifo.hpp
	c++ $(CFLAGS) -c tspcc.cpp

clean:
	rm -f *.o tspcc
