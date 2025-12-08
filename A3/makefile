#
# Sample makefile for creating threads library
#

CC = gcc
CFLAGS = -g -Wall 
SRC = thread.c thread_asm.s
OBJ = thread.o thread_asm.o
DS = queue
TT = thread_table
TESTS = test1 test2 test3 test4 test5 test6 test7 test11

.PHONY: clean all rebuild test
all: thread

thread: $(DS).o $(DS).h $(TT).o $(TT).h
	$(CC) -c $(SRC) $(CFLAGS)
	ar -cr libthread.a $^ $(OBJ)

$(DS).o: $(DS).c
	$(CC) -c $^ $(CFLAGS)

$(TT).o: $(TT).c
	$(CC) -c $^ $(CFLAGS)

test: thread
	@for test in $(TESTS); do \
		$(CC) -o $$test $$test.c libthread.a $(CFLAGS);\
		echo "==== $$test ====";\
		./$$test;\
		rm -f $$test;\
		echo "";\
	done

valg: thread test4a.c
	$(CC) -o test4a test4a.c libthread.a $(CFLAGS)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test4a
	rm -f test4a

clean:
	rm -f *.o libthread.a main

rebuild: clean all