
.PHONY: all
all: dsx20

dsx20: dsx20.o
	gcc -o dsx20 dsx20.o

dsx20.o: dsx20.c dsx20.h
	gcc -c -g dsx20.c dsx20.h

.PHONY: clean
clean:
	rm dsx20 *.o *.gch

.PHONY: rebuild
rebuild: clean all