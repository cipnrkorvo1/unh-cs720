#include <assert.h>
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
    assert(status == 1 && "memInitialize failed\n");
    printf("memInitialize returned %d\n", status);

    // set globalPtr
    globalPtr = memAllocate(15, NULL);
    printf("memAllocate returned %p\n", globalPtr);

    // set several locals
    int numPtrs = (100 / 15) - 1;
    printf("allocating %d additional blocks\n", numPtrs);
    for (int i = 0; i < numPtrs + 1; i++)
    {
        ptr[i] = memAllocate(15, (void (*))function);
        printf("memAllocate[%d] returned %p\n", i, ptr[i]);
    }

    // setting several values
    for (int i = 0; i < numPtrs; i++)
    {
        *ptr[i] = i * 16;
        printf("ptr[%d] value set to %ld\n", i, *ptr[i]);
    }

    // to see in memDump
    [[maybe_unused]] long a = ~0;

    printf("[driver] address of main: %p\n", main);
    printf("[driver] address of ptr[0]: %p\n", ptr);
    memDump();
    printf("[driver] dump successful\n");
}