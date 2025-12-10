#include <assert.h>
#include <stdio.h>

#include "alloc.h"

int released = 0;

void final()
{
    released++;
}

void function()
{
    [[maybe_unused]] long a = ~0;
    [[maybe_unused]] void *ptr = memAllocate(150, final);
    [[maybe_unused]] long b = ~1;
    //printf("<> MEMDUMP 1 <>\n");
    //memDump();
}

int main()
{
    assert(memInitialize(200) == 1);

    // finalizer = NULL
    void *ptr = memAllocate(180, NULL);
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