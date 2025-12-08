
.PHONY: all
all: binary_info

binary_info: binary_info.o
	gcc -o binary_info binary_info.o elf.h

binary_info.o: binary_info.c elf.h
	gcc -c -g binary_info.c elf.h

.PHONY: rebuild
rebuild: clean all

.PHONY: clean
clean:
	rm -f binary_info.o binary_info elf.h.gch