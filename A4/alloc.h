// interface for alloc

int memInitialize(unsigned long size);

// size: number of words to allocate
// returns pointer on success, NULL on failure
void *memAllocate(unsigned long size, void (*finalize)(void *));

void memDump(void);