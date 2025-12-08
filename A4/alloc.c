#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

#define ALLOC_BIT 0x8000000000000000
#define MARK_BIT  0x4000000000000000
#define SIZE_MASK 0x3FFFFFFFFFFFFFFF

extern long __data_start[];
extern long _end[];

static long *_stack_top;
static long *_frame_bottom;

typedef struct Block {
    unsigned long info;
    struct Block *next;
    void *finalizer;
} block_t;

int initialized = 0;    // 0: NO, 1: YES, -1: FAILED
block_t *heap = NULL;
unsigned long total_heap_size = 0;

#define BLOCK_SIZE (sizeof(block_t) / 8)

static unsigned long infoMake(int alloc, int mark, unsigned long size)
{
    return ((long)alloc << 63) | ((long)(mark & 1) << 62) | (size & SIZE_MASK);
}

// returns the block that the ptr resides within, or NULL if invalid
static block_t* getBlock(unsigned long ptr)
{
    if (initialized != 1 || !heap) return NULL;
    if (!((unsigned long)heap) - 1 <= ptr && ((unsigned long)heap) + (total_heap_size * sizeof(long)) >= ptr)
    {
        return NULL;
    }
    block_t *cur = heap;
    block_t *prev = NULL;
    while (cur && (unsigned long)cur < ptr)
    {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return NULL;
    return prev;
}

// function to check whether ptr points to allocated data
static char pointsToAllocatedData(unsigned long ptr)
{
    block_t *block = getBlock(ptr);
    if (!block) return 0;
    return (block->info & ALLOC_BIT) ? 1 : 0;
}

static int markAndSweep()
{
    // search globals for references
    

    // search stack for references

    return 0;
}

int memInitialize(unsigned long size)
{
    if (initialized) return 0;      // cannot call more than once; failure
    initialized = -1;               // change to 1 when this succeeds
    
    if (size <= 0) return 0;        // size must be greater than zero
    if (size > (unsigned long)SIZE_MASK) return 0; // size cannot be larger than the mask can handle
    total_heap_size = size * 1.2;
    heap = calloc(8, total_heap_size);    // allocate memory for heap, put into block
    if (!heap) return 0;            // failed to allocate memory

    heap->info = infoMake(0, 0, total_heap_size - BLOCK_SIZE);
    heap->next = NULL;
    heap->finalizer = NULL;         // TODO uninitialize?

    // set stack vars now in case memAllocate is never called
    __asm("\t mov %%rsp,%0\n\t" : "=r"(_stack_top));
    __asm("\t mov %%rbp,%0\n\t" : "=r"(_frame_bottom));

    // success
    initialized = 1;
    return 1;
}

void *memAllocate(unsigned long size, void (*finalize)(void *))
{
    if (initialized != 1) return NULL;

    // set stack vars for memDump and searching
    __asm("\t mov %%rsp,%0\n\t" : "=r"(_stack_top));
    __asm("\t mov %%rbp,%0\n\t" : "=r"(_frame_bottom));

    // algorithm:
    // search through blocks to find the first block which is:
    // - free
    // - large enough
    // keep track of previous block to insert the new block into the linked list
    // reduce the size of the previous block
    // TODO: no available block found (begin garbage collection)

    block_t *cur = heap;
    while (cur)
    {
        if ((cur->info & SIZE_MASK) >= size - BLOCK_SIZE && !(cur->info & ALLOC_BIT))
        {
            // insert new block here
            break;
        }
        cur = cur->next;
    }
    if (!cur)
    {
        printf("[ERROR] no space available\n");
        return NULL;
    }
    block_t *new = (block_t *)((long *)cur + BLOCK_SIZE + size);
    new->info = infoMake(0, 0, (cur->info & SIZE_MASK) - size - BLOCK_SIZE);
    new->next = cur->next;
    new->finalizer = NULL;

    cur->info = infoMake(1, 0, size);
    cur->next = new;
    cur->finalizer = finalize;

    return (char *)cur + sizeof(block_t);   // return after the block
}

void memDump(void)
{
    if (initialized != 1) return;
    if (sizeof(long) != 8UL) { fprintf(stderr, "long is not 64 bits!\n"); return; }

    // get all needed registers
    //printf("[INFO] address of GC_getRegisters: %p\n", GC_getRegisters);
    //printf("[INFO] address of memDump: %p\n", memDump);
    long rbx, rsi, rdi;
    __asm("\t mov %%rbx,%0\n\t" : "=r"(rbx));
    __asm("\t mov %%rsi,%0\n\t" : "=r"(rsi));
    __asm("\t mov %%rdi,%0\n\t" : "=r"(rdi));

    long global_length = ((long)_end - (long)__data_start) / sizeof(long);
    printf("Globals: \nstart  = %.16lx\nend    = %.16lx\nlength = %ld words\n\n", 
        (long)__data_start, (long)_end, global_length);
    printf("    %16s: %16s\n", "Address", "Value");
    for (int i = 0; i < global_length; i++)
    {
        printf("%20p: %16lx%s\n", &(__data_start[i]), __data_start[i],
            pointsToAllocatedData(__data_start[i]) ? " *" : "  ");
    }
    printf("\nEnd globals.\n");

    printf("\nPrint stack frames.\n\n");
    int frame_count = 0;
    char printing = 1;
    long *top = _stack_top;
    long *bottom = _frame_bottom;
    while (printing)
    {
        printf("\nStack frame #%d (start=%p, end=%p):\n", ++frame_count, top, bottom);

        // check if bottom is a valid frame base first
        if (bottom <= top || !bottom) break;                    // bottom invalid value
        if (bottom == top || frame_count > 128) break;          // infinite loop
        if ((long)bottom & (sizeof(void*)-1)) break;            // check for alignment

        printf("    %16s: %16s\n", "Address", "Value");
        long *addr = top;
        while (addr <= bottom)
        {
            printf("%20p: %16lx%s\n", addr, *addr, 
                pointsToAllocatedData(*addr) ? " *" : "  ");
            addr++;
        }
        top = bottom;
        bottom = (long *)*bottom;
    }
    printf("\nEnd stack frames.\n");

    // START OLD STACK
    // // locate bottom of stack
    // char success = 0;       // for sanity checks
    // long ptr = rbp;         // duplicate rbp
    // long prev = 0;
    // long bottom = 0;
    // int frame_count = 0;    // if goes over, something broke...
    // while (frame_count++ < 1024)
    // {
    //     if (ptr <= rsp) { success = 1; break; }                 // success condition
    //     if (ptr == prev) { success = -1; break; }               // infinite loop
    //     if (ptr & (sizeof(void*)-1)) { success = -2; break; }   // check for alignment
    //     long next = *(long *)ptr;                               // try dereferencing                  
    //     prev = ptr;
    //     bottom = ptr;
    //     ptr = next;
    // }
    // if (success < 1)
    // {
    //     fprintf(stderr, "frame checking failed %d\n", success);
    //     exit(-1);
    // }
    // long stack_length = (bottom - rsp) / sizeof(long);
    // printf("\nStack Memory:\n");
    // printf("start  = %.16lx\nend    = %.16lx\nlength = %ld words\n", 
    //     rsp, bottom, stack_length);
    // printf("-- top of stack area (low address) --");
    // // dump stack (more safely)
    // long max_words = 1 << 20;
    // long words = (bottom > rsp) ? ((bottom - rsp) / sizeof(long)) : 0;
    // if (words > max_words) words = max_words;
    // for (long i = 0; i < words; ++i)
    // {
    //     long addr = rsp + i * sizeof(long);
    //     printf("\n%.16lx : %.16lx%s", addr, *(long*)addr,
    //         pointsToAllocatedData(*(long*)addr) ? " *" : "  ");
    // }
    // printf("\n-- bottom of stack (high address) --\n");
    // END OLD STACK

    printf("\nRegisters:\n");
    printf("%%rbx = %.16lx  %%rsi = %.16lx  %%rdi = %.16lx\n", rbx, rsi, rdi);

    printf("\nBlock header size: %lu words\n", BLOCK_SIZE);
    printf("Printing blocks.\n\n");
    // parse heap data
    block_t *cur = heap;
    int block_count = 0;
    while (cur)
    {
        unsigned long cur_size = cur->info & SIZE_MASK;
        printf("Block %d (%s): %ld words. ", ++block_count, 
            (cur->info) & ALLOC_BIT ? "Allocated" : "Free", cur_size);
        if (cur->info & ALLOC_BIT)
        {
            printf("Finalizer @ 0x%lx\n", (long)(cur->finalizer));   // TODO finalizer function
            printf("    %16s: %16s\n", "Address", "Value");
            // TODO skip large lines of non-members
            long *ptr = (long *)((char *)cur + sizeof(block_t));
            for (int i = 0; i < cur_size; i++)
            {
                printf("%20p: %16lx%s\n", ptr, *ptr, pointsToAllocatedData(*ptr) ? " *" : " ");
                ptr++;
            }
            printf("\n");
        }
        
        cur = cur->next;
    }
    printf("\nEnd heap.\n");
}