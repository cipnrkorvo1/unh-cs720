#include <stdio.h>
#include "thread.h"

// stress test of thread_create, thread_join and thread termination  under
// preemptive scheduling

//void thread_stats(void);

#define N 1000000
int cnt = 0;

void thread1(void* info);

int main(void)
{
  int i;

  for (i = 0; i < N; i++)
  {
    long tid = thread_create(thread1, 0);
    if (i % 2) thread_yield();
    int ret = thread_join(tid);
    if (ret)
    {
      fprintf(stderr, "thread join returned %d\n", ret);
    }
  }
  fprintf(stderr, "%d thread executions completed!\n", cnt);
  fprintf(stderr, "i is %d\n", i);
  //thread_stats();
  return 0;
}

void thread1(void* info) 
{
  cnt += 1;
}