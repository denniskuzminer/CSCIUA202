CC=gcc
CFLAGS=-g -std=gnu17 #-pedantic -std=gnu17 -Wall -Werror -Wextra

.PHONY: all
all: nyush

nyush: nyush.o 

nyush.o: nyush.c

.PHONY: clean
clean:
		rm -f *.o nyush