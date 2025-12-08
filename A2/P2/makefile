export
CC = gcc
CFLAGS = -Wall -g -pthread

LIB = libvmx20
LIBPATH = .

.PHONY: all
all: vmx20 test

.PHONY: vmx20
vmx20: $(LIB).a

$(LIB).a: vmx20.o vmx20.h
	ar -rcs $(LIB).a vmx20.o

.PHONY: test
test: testvm.o $(LIB).a
	gcc -o testvm $< -L$(CURDIR) -l:$(LIB).a

.PHONY: clean
clean:
	rm -f testvm *.o *.gch *.a

.PHONY: rebuild
rebuild: clean all	