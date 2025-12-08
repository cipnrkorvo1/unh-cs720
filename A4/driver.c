#include <stdio.h>

#include "alloc.h"

// A4 test driver
void *globalPtr = 0;
long anotherPtr = 0x23456789;

void function(void)
{
    return;
}

int main()
{
    int status;
    long *ptr[10] = {};
    status = memInitialize(100);
    printf("memInitialize returned %d\n", status);

    globalPtr = memAllocate(15, NULL);
    printf("memAllocate returned %p\n", globalPtr);

    int numPtrs = (100 / 15) - 1;
    for (int i = 0; i < numPtrs + 1; i++)
    {
        ptr[i] = memAllocate(15, (void (*))function);
        printf("memAllocate[%d] returned %p\n", i, ptr[i]);
    }

    for (int i = 0; i < numPtrs; i++)
    {
        *ptr[i] = i * 16;
        printf("ptr[%d] value set to %ld\n", i, *ptr[i]);
    }

    [[maybe_unused]] long a = ~0;

    printf("[driver] address of main: %p\n", main);
    printf("[driver] address of ptr[0]: %p\n", ptr);
    memDump();
    printf("[driver] dump successful\n");
}