#include <stdio.h>

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)

extern long thread_create(void (*)(void*), void*);
extern void thread_yield();
extern void thread_cleanup();

void thread1(void* info);

int main(void)
{
    thread_create(thread1, "info passed correctly");
    status(0,1);
    thread_yield();
    thread_yield();
    printf("** Only running thread yielded and continued\n");
    status(1,1);
    thread_cleanup();
}

void thread1(void* info) 
{
    printf("** Thread 1 running after main yielded\n");
}