#include <stdio.h>

#include "thread.h"

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)

thread_mutex_t lock;

void thread1(void *info)
{
    status((int)(long)info, 5);
    thread_mutex_lock(&lock);
    status((int)(long)info + 1, 5);
    thread_yield();
}

int main()
{
    long t1 = thread_create(thread1, (void*)1);
    long t2 = thread_create(thread1, (void*)3);
    thread_mutex_init(&lock);
    printf("Create mutex deadlock\n");
    status(0,5);
    thread_join(t1);
    thread_join(t2);
    status(5,5);
}