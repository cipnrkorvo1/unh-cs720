CC = gcc
CFLAGS = -g -Wall
SRC = alloc.c
OBJ = alloc.o

TESTS = misc_driver finalize_test error_finalize_memallocate ref_to_block_in_block recursive_stack_check

.PHONY: all clean rebuild test cmptest
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

# just compiles the tests into execs
cmptest: alloc
	@for test in $(TESTS); do \
		$(CC) -o $$test $$test.c liballoc.a $(CFLAGS);\
	done
	@echo "compiled tests: $(TESTS)"

clean:
	rm -f *.o liballoc.a $(TESTS)

rebuild: clean all