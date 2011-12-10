
all: bilebio

bilebio: bilebio.o
    gcc bilebio.o -o bilebio -lm -lcurses

bilebio.o: bilebio.c
    gcc -c -g -ansi -pedantic -Wall -Werror bilebio.c

