#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

int fail = 0;

void final()
{
    fail = 1;
}

int main()
{
    assert(memInitialize(100) == 1);
    // stack(A) -> stack(B) -> C
    long *A = calloc(1, sizeof(long));
    long *B = calloc(1, sizeof(long));
    long *C = memAllocate(100, final);

    *A = (long)B;
    *B = (long)C;
    B = NULL;
    C = NULL;

    assert(memAllocate(100, NULL) == NULL && "Recursive reference to C not found.\n");
    fail ? printf("Failure!\n") : printf("Success!\n");

    // eof
    free(*(long **)A);
    free(A);
}