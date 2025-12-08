#include <stdio.h>

// simple test for thread_create and thread_yield
//  create a thread, yield to it and it yields back
//  thread termination is not tested as main thread exits first

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)

extern long thread_create(void (*)(void*), void*);
extern void thread_yield();
extern void thread_cleanup();

void thread1(void* info);

int main(void)
{
    thread_create(thread1, "info passed correctly");
    status(0,2);
    thread_yield();
    status(2,2);
    thread_cleanup();
}

void thread1(void* info) 
{
    status(1,2);
    fprintf(stderr, "%s\n", (char*)info);
    thread_yield();
    status(3,2); /* error if program gets here */
}