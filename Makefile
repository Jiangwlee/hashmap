CC = g++
FLAGS = -DDEBUG
TARGETDIR = build
INCLUDE = -Iinclude

vpath %.h include

hash_table : main.o
	$(CC) -o hash_table main.o

main.o : main.cpp
	$(CC) $(FLAGS) $(INCLUDE) -c main.cpp

clean : 
	rm -f *.o hash_table
