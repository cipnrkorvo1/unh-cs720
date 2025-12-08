#include <assert.h>
#include <stdio.h>

#include "alloc.h"

void func(void *)
{
    printf("If this printed, something is very wrong.\n");
}

void finalize(void *)
{
    void *ptr = memAllocate(sizeof(long), func);
    *(long *)ptr = 0xffaaffaa;
    printf("If this printed, memAllocate did not fail like it should have.\n");
}

int main(void)
{
    assert(memInitialize(1000) == 1 && "memInitialize failed.");

    [[maybe_unused]] void *ptr = memAllocate(100, finalize);

    // take up rest of space
    void *pointers[100];
    int i = 0;
    while (i < 100)
    {
        pointers[i] = memAllocate(80, NULL);
        if (pointers[i] == NULL) break;
        i++;
    }
    assert(i < 100);

    // remove reference to ptr
    ptr = NULL;
    // try to allocate again
    pointers[i] = memAllocate(80, NULL);
    assert(pointers[i] != NULL && "GC did not deallocate released block when it should have.\n");

    // end?

}