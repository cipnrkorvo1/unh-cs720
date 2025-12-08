// interface for alloc

int memInitialize(unsigned long size);
void *memAllocate(unsigned long size, void (*finalize)(void *));

void memDump(void);