#include <assert.h>
#include <stdio.h>

#include "alloc.h"

int fail = 0;

void final()
{
    fail = 1;
    printf("If this prints, the reference was not found in the heap (FAILURE).\n");
}

int main()
{
    assert(memInitialize(200) == 1);
    void *A = memAllocate(100, final);
    long *B = memAllocate(100, NULL);

    *(B + 5) = (long)A;     // store ref to A inside of B
    A = NULL;               // remove outward ref to A

    // try to allocate an additional 100 word;
    // this should fail to return a pointer
    assert(memAllocate(100, NULL) == NULL && "A was deallocated when its reference should have been found inside of B.\n");
    fail ? printf("Failure!\n") : printf("Success!\n");
}