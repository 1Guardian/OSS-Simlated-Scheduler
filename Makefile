#Vars
CC=g++
CFLAGS=-std=c++11 -pthread
OBJECTS=testsim oss
DEPS=config.h

#Make both bin objects
all: $(OBJECTS)

testsim:
	$(CC) $(CFLAGS) -o testsim testsim.cpp $(DEPS)

oss:
	$(CC) $(CFLAGS) -o oss oss.cpp $(DEPS)

#Clean
clean: 
	rm $(OBJECTS) 