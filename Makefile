
all: bilebio

clean:
	rm bilebio.o bilebio

bilebio: bilebio.o
	gcc bilebio.o -o bilebio -lm -lcurses

bilebio.o: bilebio.c
	gcc -c -g -ansi -pedantic -Wall -Wextra bilebio.c

