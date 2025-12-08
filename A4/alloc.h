// interface for alloc

int memInitialize(unsigned long size);

// returns pointer on success, NULL on failure
void *memAllocate(unsigned long size, void (*finalize)(void *));

void memDump(void);