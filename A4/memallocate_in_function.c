#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

int released = 0;

void final()
{
    released++;
}

void function()
{
    long a = ~0;
    long *ptr = memAllocate(150, final);
    if (!ptr) 
    {
        printf("memAllocate in function failed.\n");
        exit(-1);
    }
    long b = ~1;
    (void)(a + b);
}

int main()
{
    assert(memInitialize(200) == 1);

    // finalizer = NULL
    long *ptr = memAllocate(180, NULL);
    if (!ptr)
    {
        printf("first memAllocate in main failed.\n");
        exit(-1);
    }
    ptr = NULL;
    // immediately dump ptr away

    // memAllocate in function
    function();
    //printf("<> MEMDUMP 2 <>\n");
    //memDump();

    // original block should have gone out of scope; trigger coalesce
    assert(memAllocate(200, NULL) != NULL);
    printf("%s!\n", released ? "Success" : "Failure");
}