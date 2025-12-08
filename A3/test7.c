#include <stdio.h>

#include "thread.h"

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)
#define TOTAL 8

thread_mutex_t lock;

thread_cond_t cond;

void thread1(void *info)
{
    thread_mutex_lock(&lock);
    status(1, TOTAL);
    thread_cond_wait(&cond, &lock);
    status(7, TOTAL);
}

void thread2(void *info)
{
    status(2, TOTAL);
    thread_yield();
    status(5, TOTAL);
}

void thread3(void *info)
{
    thread_mutex_lock(&lock);
    status(3, TOTAL);
    thread_mutex_unlock(&lock);
    status(4, TOTAL);
}

int main()
{
    thread_mutex_init(&lock);
    thread_cond_init(&cond);
    
    long t1 = thread_create(thread1, NULL);
    long t2 = thread_create(thread2, NULL);
    long t3 = thread_create(thread3, NULL);

    status(0, TOTAL);
    thread_join(t2);
    thread_join(t3);
    status(6, TOTAL);
    thread_cond_signal(&cond);
    thread_join(t1);
    status(8, TOTAL);

    thread_cleanup();
}