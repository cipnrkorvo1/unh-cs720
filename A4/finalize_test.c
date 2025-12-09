#include <assert.h>
#include <stdio.h>

#include "alloc.h"

void final()
{
    printf("Finalize function success!\n");
}

int main()
{
    assert(memInitialize(300) == 1);
    [[maybe_unused]] void *ptr = memAllocate(100, final);

    ptr = NULL;
    // exhaust memory
    void *arr[100];
    int i = 0;
    for (; i < 100; i++)
    {
        arr[i] = memAllocate(90, NULL);
        if (arr[i] == NULL) break;
    }
    assert(i < 100);
}