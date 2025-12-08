#include <stdio.h>

#include "thread.h"

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)

thread_mutex_t lock1;
thread_mutex_t lock2;

void thread1(void *info)
{
    thread_mutex_lock(&lock1);
    printf("should not print (t1)\n");
    thread_mutex_unlock(&lock1);
}

void thread2(void *info)
{
    thread_mutex_lock(&lock1);
    thread_join(*(long*)info);
    thread_mutex_unlock(&lock1);
    printf("should not print (t2)\n");
}

void thread3(void *info)
{
    thread_mutex_lock(&lock2);
    printf("should not print (t3)\n");
    thread_mutex_unlock(&lock2);
}

int main()
{
    thread_mutex_init(&lock1);
    thread_mutex_init(&lock2);
    long t1 = -1, t2 = -1, t3 = -1;
    t2 = thread_create(thread2, &t3);
    t1 = thread_create(thread1, NULL);
    t3 = thread_create(thread3, NULL);

    thread_mutex_lock(&lock2);
    printf("parent acquired lock2\n");
    printf("parent join t1 (1/3): %d\n", thread_join(t1));
    printf("parent join t2 (2/3): %d\n", thread_join(t2));
    printf("parent join t3 (3/3): %d\n", thread_join(t3));
    thread_mutex_unlock(&lock2);
    printf("parent released lock2\n");

    thread_cleanup();
}