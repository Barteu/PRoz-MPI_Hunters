SOURCES=$(wildcard *.c)
HEADERS=$(SOURCES:.c=.h)
FLAGS=-DDEBUG -g -pthread

all: main

main: $(SOURCES) $(HEADERS)
	mpicc $(SOURCES) $(FLAGS) -o main

clear: clean

clean:
	rm main a.out

run: main
	mpirun -np 8 ./main
