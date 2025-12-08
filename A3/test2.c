#include <stdio.h>
#include "thread.h"

#define status(part,total) fprintf(stderr, "(%d/%d)\n", part, total)

void thread1(void *info)
{
    status(1,6);
    thread_join(2);
    status(5,6);
}

void thread2(void *info)
{
    status(2,6);
    thread_join(3);
    status(4,6);
}

void thread3(void *info)
{
    status(3,6);
}

void dlock(void *info)
{
    status(thread_join(0), (int)thread_self());
}

void dlock2(void *info)
{
    status(thread_join(*(long *)info), (int)thread_self());
}

int main(void)
{
    printf("P1: chained thread_join()\n");
    thread_create(thread1, NULL);
    thread_create(thread2, NULL);
    thread_create(thread3, NULL);
    status(0,6);
    printf("Join: %d\n", thread_join(1));
    status(6,6);

    printf("Join with nonexistent thread: %d\n", thread_join(-1));

    printf("P2: deadlocks\n");
    printf("Self join: %d\n", thread_join(0));
    long t1 = thread_create(dlock, NULL);
    printf("Double join: %d\n", thread_join(t1));
    long tx = thread_create(dlock, NULL);
    long tid[6] = {tx, 0, 0, 0, 0, 0};
    for (int i = 1; i < 6; i++)
    {
        tid[i] = thread_create(dlock2, &tid[i-1]);
    }
    printf("Long chained join: %d\n", thread_join(tid[5]));
    printf("Cleaning up\n");
    thread_cleanup();
    return 0;
}