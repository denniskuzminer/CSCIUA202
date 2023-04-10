CC=gcc
CFLAGS= -g -std=gnu17 -O -l crypto#-pedantic -std=gnu17 -Wall -Werror -Wextra

.PHONY: all
all: nyufile

nyuenc: nyufile.o 

nyuenc.o: nyufile.c

.PHONY: clean
clean:
				rm -f *.o nyufile