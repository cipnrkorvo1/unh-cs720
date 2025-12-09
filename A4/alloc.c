#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

#define DEBUG 0

#define ALLOC_BIT 0x8000000000000000
#define MARK_BIT  0x4000000000000000
#define SIZE_MASK 0x3FFFFFFFFFFFFFFF

extern long __data_start[];
extern long _end[];

static long *_stack_top;            // low address
static long *_frame_bottom;         // high address

typedef struct Block {
    unsigned long info;
    struct Block *next;
    void *finalizer;
} block_t;
/**
 * TODO: Block->next can be calculated from Block->info & SIZE_MASK
 * Block->next is therefore redundant.
 * Future update should remove it if possible and instead calculate from the size.
 */

static int initialized = 0;    // 0: NO, 1: YES, -1: FAILED
static block_t *heap = NULL;
static unsigned long total_heap_size = 0;  // number of words
static char in_finalize_call = 0;

#define BLOCK_SIZE (sizeof(block_t) / 8)

// SMALL HELPERS

static unsigned long infoMake(int alloc, int mark, unsigned long size)
{
    return ((long)alloc << 63) | ((long)(mark & 1) << 62) | (size & SIZE_MASK);
}

static char inGlobalRange(unsigned long val)
{
    return (unsigned long)__data_start <= val && val < (unsigned long)_end;
}
static char inStackRange(unsigned long val)
{
    return (unsigned long)_frame_bottom > val && val >= (unsigned long)_stack_top;
}
static char inHeapRange(unsigned long val)
{
    //return (unsigned long)heap <= val && val < (unsigned long)(heap + total_heap_size);
    return (unsigned long)heap <= val && val < (unsigned long)heap + (total_heap_size * sizeof(long));
}
[[maybe_unused]] static char inValidMemory(unsigned long val)
{
    return inGlobalRange(val) || inStackRange(val) || inHeapRange(val);
}

// END SMALL HELPERS

// returns the block that the ptr resides within, or NULL if invalid
static block_t* getBlock(unsigned long ptr)
{
    if (initialized != 1 || !heap) {
        if (DEBUG) printf("[getBlock] initialized != 1 || !heap\n");
        return NULL;
    }
    if (ptr < (unsigned long)heap || ptr >= (unsigned long)((long *)heap + total_heap_size))
    {
        //if (DEBUG) printf("[getBlock] ptr is outside of heap\n");
        return NULL;
    }

    block_t *cur = heap;
    block_t *prev = heap;
    while (cur && (unsigned long)cur < ptr)
    {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) {
        if (DEBUG) printf("[getBlock] cur == NULL; did not find a block\n");
        return NULL;
    }
    //if (DEBUG) printf("[getBlock] ptr %p located in %p\n", (void*)ptr, prev);
    return prev;
}

// function to check whether ptr points to allocated data
// TODO: check down linked list of references? perhaps another function
static char pointsToAllocatedData(unsigned long ptr)
{
    block_t *block = getBlock(ptr);
    if (!block) return 0;
    return (block->info & ALLOC_BIT) ? 1 : 0;
}

// mark and sweep protocol
static int markAndSweep()
{

    if (DEBUG) printf("[DEBUG] markAndSweep() begin searching globals\n");

    // STEP 1: MARK ALL USED BLOCKS
    // 1a. search globals for references
    long global_length = ((long)_end - (long)__data_start) / sizeof(long);
    int blocks_marked = 0;
    for (int i = 0; i < global_length; i++)
    {
        unsigned long data = __data_start[i];
        int iterations_left = 100;                              // limit iterations in case of infinite loop   
        while (data && iterations_left-- > 0)
        {
            if (inHeapRange(data))
            {
                // figure out which block this is pointing to
                block_t *block = getBlock(data);
                if (data == (unsigned long)heap)
                {
                    if (DEBUG) printf("data == heap\n");
                    // ignore! -- this is most likely the reference in this file!
                    data = 0;   // end loop
                    continue;
                }
                // only do something if mark bit is not set yet
                if ((block->info & MARK_BIT) == 0 && (block->info & ALLOC_BIT))
                {
                    // set the block's mark bit
                    block->info |= MARK_BIT;
                    blocks_marked++;
                    data = 0;    // end loop
                }
                else
                {
                    data = *(long *)data;
                }
            }
            else if (inGlobalRange(data) || inHeapRange(data))
            {
                // data points to valid address, keep looking
                data = *(long *)data;
            }
        }
    }
    if (DEBUG) printf("[ M&S ] globals: marked %d blocks.\n", blocks_marked);
    // 1b. search stack for references
    if (DEBUG) printf("[ M&S ] begin searching stack\n");
    blocks_marked = 0;
    int frame_count = 0;
    char sweeping = 1;
    long *top = _stack_top;
    long *bottom = _frame_bottom;
    while (sweeping)
    {
        // check if bottom is a valid frame base first
        if (bottom <= top || !bottom) break;                    // bottom invalid value
        if (bottom == top || frame_count > 128) break;          // infinite loop
        if ((long)bottom & (sizeof(void*)-1)) break;            // check for alignment

        long *addr = top;
        while (addr <= bottom)
        {
            unsigned long data = *addr;
            int iterations_left = 100;                          // limit iterations in case of infinite loops
            while (data && iterations_left-- > 0)
            {
                if (inHeapRange(data))
                {   
                    // figure out which block this is pointing to
                    block_t *block = getBlock(data);
                    // only do something if mark bit is not set yet
                    if ((block->info & MARK_BIT) == 0 && (block->info & ALLOC_BIT))
                    {
                        // set the block's mark bit
                        block->info |= MARK_BIT;
                        blocks_marked++;
                        data = 0;    // end loop
                    }
                    else
                    {
                        data = *(long *)data;
                    }
                }
                else if (inGlobalRange(data) || inHeapRange(data))
                {
                    // data points to valid address, keep looking
                    data = *(long *)data;
                }
            }
            addr++;
        }
        top = bottom;
        bottom = (long *)*bottom;
    }
    if (DEBUG) printf("[ M&S ] stack: marked %d blocks.\n", blocks_marked);
    blocks_marked = 0;
    // 1c. search heap for references
    // TODO
    // if a reference to block A is located in an unmarked block B, the ref is invalid
    // but if block C is marked and has a ref to block B then B is marked, then the ref to A is valid!
    // must loop until no additional blocks are found.
    {
    block_t *cur = heap;
    char marked_any = 1;
    while (marked_any)
    {
        cur = heap;
        marked_any = 0;
        while (cur)
        {
            long *ptr = ((long *)cur) + BLOCK_SIZE;
            long *end = ((long *)cur) + (cur->info & SIZE_MASK);
            while (ptr < end)
            {
                unsigned long data = *ptr;
                if (inHeapRange(data))
                {
                    block_t *block = getBlock(data);
                    // only do something if mark bit is not set yet
                    if ((block->info & MARK_BIT) == 0 && (block->info & ALLOC_BIT))
                    {
                        // set the block's mark bit
                        block->info |= MARK_BIT;
                        blocks_marked++;
                        marked_any = 1;
                        data = 0;    // end loop
                    }
                    else
                    {
                        data = *(long *)data;
                    }
                }
            }
            cur = cur->next;
        }
    }

    }
    // STEP 2: SET ALL UNUSED BLOCKS TO 'FREE' (UNSET ALLOC BIT)
    if (DEBUG) printf("[ M&S ] begin releasing unused/unmarked blocks\n");
    block_t *cur = heap;
    block_t *prev = NULL;
    unsigned long words_freed = 0;
    while (cur)
    {
        if (DEBUG)
        {
            printf("[DEBUG] Block: info=%.16lx\n", cur->info);
        }
        if (cur->info & MARK_BIT)
        {
            // cur is marked; in use
            cur->info &= ~MARK_BIT;  // unset mark bit
            cur = cur->next;
            prev = NULL;
            continue;
        }

        // cur is not marked; no longer in use
        if (cur->info & ALLOC_BIT)
        {
            // unset alloc bit; set block as freed
            cur->info &= ~ALLOC_BIT;    
            words_freed += (cur->info & SIZE_MASK);
            // block is surely freed; call finalizer
            in_finalize_call = 1;
            ((void (*)(void))cur->finalizer)();
            in_finalize_call = 0;
        }
        
        // coalesce with prev block
        if (prev)
        {
            // set prev block size to old + (block struct size) + new
            long prev_size = prev->info & SIZE_MASK;
            prev->info &= ~SIZE_MASK;
            prev_size += BLOCK_SIZE + (cur->info & SIZE_MASK);
            prev->info |= prev_size & SIZE_MASK;
            // this block should remain `prev`
            prev->next = cur->next;
            // prev->finalizer = cur->finalizer;
            // ^ block is already free; finalizer should not be called! (probably!!)
            words_freed += BLOCK_SIZE;
        }
        else
        {
            // allow next block to coalesce with this one if also freed
            prev = cur;
        }
        // advance to the next block
        cur = cur->next;
    }
    if (DEBUG) printf("[DEBUG] %lu words freed\n", words_freed);

    return 0;
}

int memInitialize(unsigned long size)
{
    if (DEBUG) printf("[DEBUG] memInitialize(%ld)\n", size);
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

static block_t *nextBlockOfSize(unsigned long size)
{
    block_t *cur = heap;
    while (cur)
    {
        if ((cur->info & SIZE_MASK) >= size - BLOCK_SIZE && !(cur->info & ALLOC_BIT))
        {
            // return this block
            return cur;
        }
        cur = cur->next;
    }
    // no block of that size was found
    return NULL;
}

void *memAllocate(unsigned long size, void (*finalize)(void *))
{
    if (DEBUG) printf("[DEBUG] memAllocate(%ld, %p)\n", size, finalize);
    if (initialized != 1) return NULL;
    if (in_finalize_call)
    {
        fprintf(stderr, "memAllocate called in a finalizer.\n");
        exit(-1);
    }

    // set stack vars for memDump and searching
    __asm("\t mov %%rsp,%0\n\t" : "=r"(_stack_top));
    __asm("\t mov %%rbp,%0\n\t" : "=r"(_frame_bottom));

    // algorithm:
    // search through blocks to find the first block which is:
    // - free
    // - large enough
    // keep track of previous block to insert the new block into the linked list
    // reduce the size of the previous block

    block_t *alloced = nextBlockOfSize(size);   // <-- the block we will return to the caller
    if (!alloced)
    {
        // no available block found. execute mark and sweep
        [[maybe_unused]] int result = markAndSweep();
        // TODO result
        // try to find a block again
        alloced = nextBlockOfSize(size);
        if (!alloced)
        {
            if (DEBUG) printf("[ERROR] no space available\n");
            return NULL;
        }
        // block was found!
    }

    // we know that (long *)alloced + BLOCK_SIZE + "alloced->size" => alloced->next
    // AND that `size` <= "alloced->size"
    // two cases:
    //      `size` is within BLOCK_SIZE of "alloced->size"  =>
    //          cannot generate a new block, keep size (and next) the same
    //      `size` is much smaller than "alloced->size"     =>
    //          create a new block and set its size to the difference (account for block size)

    unsigned long alloced_size = alloced->info & SIZE_MASK;
    // new_size = alloced_size - size - BLOCK_SIZE
    if (alloced_size - size >= BLOCK_SIZE + 1)       // i will not allow a block of zero to be generated.
    {
        // can accomodate for a new block
        // "create" a new block
        block_t *new = (block_t *)((long *)alloced + BLOCK_SIZE + size);
        new->info = infoMake(0, 0, alloced_size - size - BLOCK_SIZE);
        new->next = alloced->next;
        new->finalizer = NULL;
        // modify alloced to be what we want
        alloced->info = infoMake(1, 0, size);
        alloced->next = new;
        alloced->finalizer = finalize;
    } else {
        // we cannot accomodate for a new block
        // instead retain "alloced->size"; give the caller a slightly larger block (this is OK)
        alloced->info = infoMake(1, 0, alloced_size);
        // no change to alloced->next
        alloced->finalizer = finalize;
    }

    if (DEBUG) printf("[DEBUG] allocated a block of size %ld (%ld)+(%ld) words\n", (alloced->info & SIZE_MASK) + BLOCK_SIZE, BLOCK_SIZE, alloced->info & SIZE_MASK);
    return (char *)alloced + sizeof(block_t);   // return after the block
}

void memDump(void)
{
    if (DEBUG) printf("[DEBUG] memDump()\n");
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
            printf("Finalizer @ 0x%lx\n", (long)(cur->finalizer));
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