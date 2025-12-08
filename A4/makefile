CC = gcc
CFLAGS = -g -Wall
SRC = alloc.c
OBJ = alloc.o

TESTS = driver error_finalize_memallocate

.PHONY: all clean rebuild test
all: alloc

alloc:
	$(CC) -c $(SRC) $(CFLAGS)
	ar -cr liballoc.a $^ $(OBJ)

test: alloc
	@for test in $(TESTS); do \
		$(CC) -o $$test $$test.c liballoc.a $(CFLAGS);\
		echo "==== $$test ====";\
		./$$test;\
		rm -f $$test;\
		echo "";\
	done
	@$(MAKE) clean

clean:
	rm -f *.o liballoc.a

rebuild: clean all