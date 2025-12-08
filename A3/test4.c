#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "thread.h"

typedef struct INFO {
    thread_mutex_t *lock;
    int *counter;
} info_t;

void work(void *info)
{
    info_t *inf = (info_t *)info;
    for (int i = 0; i < 100; i++)
    {
        thread_mutex_lock(inf->lock);
        int num = *(inf->counter);
        if (rand() % 7 == 0) thread_yield();
        *(inf->counter) = num + 1;
        thread_mutex_unlock(inf->lock);
        if (rand() % 7 == 0) thread_yield();
    }
}

int main()
{
    srand(time(NULL));
    thread_mutex_t *lock = malloc(sizeof(thread_mutex_t));
    if (thread_mutex_init(lock) == 0) {
        printf("Error initializing mutex\n");
        exit(-1);
    }
    const int THREADS = 10;
    int counter = 0;
    long tid[THREADS] = {};
    info_t *inf = malloc(sizeof(info_t) * THREADS);
    for (int i = 0; i < THREADS; i++)
    {
        inf[i].counter = &counter;
        inf[i].lock = lock;
        tid[i] = thread_create(work, &inf[i]);
    }
    for (int i = 0; i < THREADS; i++)
    {
        thread_join(tid[i]);
    }
    printf("counter = %d\n", counter);
    thread_cleanup();
    free(lock);
    return 1;
}