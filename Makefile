CC=g++
CFLAGS=-std=c++0x -g -O2

all: 
	$(CC) $(CFLAGS) model.cc -o model -lsimlib -lm

run:
	./model
	
