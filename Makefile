
all: bilebio

clean:
	rm -f bilebio.o borg.o bilebio

bilebio: bilebio.o
	gcc $^ -o $@ -lm -lcurses

bilebio-borg: bilebio-borg.o borg.o
	gcc $^ -o $@ -lm -lcurses

bilebio.o: bilebio.c
	gcc -c -g -ansi -pedantic -Wall -Wextra bilebio.c

bilebio-borg.o: bilebio.c
	gcc -DRUN_BORG -c -g -ansi -pedantic -Wall -Wextra $^ -o $@

borg.o: borg.c
	gcc -c -g --std=c99 -pedantic -Wall -Wextra borg.c
