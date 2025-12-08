export
CC = gcc
CFLAGS = -Wall -g

.PHONY: all
all: linkx20

linkx20: linkx20.o linkx20.h

.PHONY: clean
clean:
	rm -f linkx20 *.o *.gch

.PHONY: rebuild
rebuild: clean all