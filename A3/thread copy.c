#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "thread.h"
#include "data_structures.h"

#define STACK_SIZE 65536

#define DEBUG 0

enum Status {
    WORKING,
    WAITING,
    DONE
};

/**
 * x86-64 passes discrete arugments in registers %rdi, %rsi, %rdx, %rcx, %r8, %r9
 * Return value is expected to be in %rax
 * Linux kernel is expected to pass the syscall number through %rax
 */

typedef struct TCB {
    // callee saved registers
    int64_t rbx;
    int64_t r12;
    int64_t r13;
    int64_t r14;
    int64_t r15;

    int64_t rbp;            // frame pointer
    int64_t rsp;            // stack pointer
    int64_t rip;            // program counter (unused :/)

    int64_t rdi;            // first argument in function call
    int64_t rsi;            // second argument in function call
    void *stack;            // only needed for freeing later
    long tid;               // thread id
    enum Status status;
    struct TCB *observer;   // TCB of thread who is waiting on this one (thread_join)
} tctrl_t;

extern void asm_yield(void *current, void *next);

static int ready = 0;
void *all_threads = NULL;
void *ready_list = NULL;
void *mutexes = NULL;
void *prev = NULL;

static struct TCB *current_thread = NULL; // TCB

static void thread_terminate(void); // function to run when thread finishes
static void destroy_thread(void *); // function to free thread

/// ==== INIT ====

static int __num_threads = -1;
static int next_tid()
{
    __num_threads += 1;
    return __num_threads;
}

static void init()
{
    if (DEBUG) printf("init()\n");
    if (init_queue(&ready_list) || init_queue(&all_threads) || init_queue(&mutexes))
    {
        fprintf(stderr, "Error initializing a list\n");
        exit(-1);
    }
    struct TCB *parent = malloc(sizeof(struct TCB));
    __num_threads = -1;
    parent->tid = next_tid();
    parent->status = WORKING;
    parent->stack = NULL;
    push(all_threads, parent);
    push(ready_list, parent);
    // set parent as current thread, assuming list was empty
    current_thread = pop(ready_list);

    ready = 1;
}

long thread_self(void)
{
    if (!ready) init();
    return current_thread->tid;
}

long thread_create(void (*work)(void *), void *info)
{
    if (!ready) init();

    struct TCB *thread = malloc(sizeof(struct TCB));
    thread->tid = next_tid();
    thread->status = WORKING;
    thread->stack = malloc(STACK_SIZE);

    // init registers
    thread->rbx = 0;
    thread->r12 = 0;
    thread->r13 = 0;
    thread->r14 = 0;
    thread->r15 = 0;

    // create empty stack
    thread->rsp = (int64_t)(thread->stack + STACK_SIZE);
    thread->rbp = thread->rsp;

    // push cleanup address to stack
    thread->rsp -= 8;
    *(void **)thread->rsp = thread_terminate;

    // push work function address to stack
    thread->rsp -= 8;
    *(void **)thread->rsp = work;
    // account for asm_yield frame being created on previous thread (will be immediately popped)
    thread->rbp = thread->rsp - 8;

    // work function arguments
    thread->rdi = *(int64_t *)&info;
    thread->rsi = 0;

    push(all_threads, thread);
    push(ready_list, thread);
    if (DEBUG) printf("created thread %ld\n", thread->tid);
    return thread->tid;
}

/// ==== THREAD YIELD ====

void thread_yield(void)
{
    if (DEBUG) printf("%ld: thread_yield()\n", current_thread->tid);
    if (!ready) init();

    struct TCB* old = current_thread;         // save current thread
    struct TCB* next = pop(ready_list);       // move next thread up
    if (next == NULL)                         // no thread was waiting...
    {
        if (DEBUG) printf("no waiting thread... cannot yield\n");
        if (old->status != WORKING)
        {
            // fatal error
            fprintf(stderr, "ERROR: deadlock - no threads available to work\n");
            exit(-1);
        }
        return; 
    }
    if (next->status != WORKING)
    {
        // thread not ready to work
        thread_yield();
        return;
    }
    current_thread = next;
    if (DEBUG) printf("thread %ld yielding to thread %ld\n", old->tid, current_thread->tid);
    if (old->status == WORKING)
    {
        push(ready_list, old);              // push old thread to ready_list
    }
    asm_yield(old, current_thread);     // context switch to new thread 
}

/// ==== THREAD JOIN ==== ///

static void* getThreadById(long target)
{
    void *iter = iterator(all_threads);
    struct TCB *current = NULL;
    while ((current = next(&iter)))
    {
        if (current->tid == target) return current;
    }
    return NULL;
}

/**
 * Used by thread_join() to detect if a deadlock has occurred.
 * i.e. checks for cycles in the linked list of observers from current_thread
 */
static int detect_deadlock()
{
    struct TCB *slow = current_thread, *fast = current_thread;
    while (slow && fast && fast->observer)
    {
        slow = slow->observer;
        fast = fast->observer->observer;
        if (slow && fast && slow->tid == fast->tid)
        {
            return 1;
        }
    }
    return 0;
}

/*
The thread_join primitive waits for the thread specified by its parameter to terminate.
If that thread has already terminated, then thread_join returns immediately.

When "waking up" a thread that has been waiting to join, put the waking thread on the
end of the ready list.

On success, thread_join returns 0; on error, it returns an error code:
    -1: A deadlock was detected (e.g., two threads tried to join with each other); or
        the given thread ID specifies the calling thread.
    -2: Another thread is already waiting to join with this thread.
    -3: No thread with the given thread ID could be found.

If multiple threads simultaneously try to join with the same thread, the results are
undefined.
Joining with a thread that has previously been joined results in undefined behavior.
*/
int thread_join(long thread_id)
{
    if (!ready) init();
    if (DEBUG) printf("Joining thread %ld with thread %ld\n", current_thread->tid, thread_id);
    // detect self-referential deadlock
    if (current_thread->tid == thread_id) return -1;
    // get target thread to join with
    struct TCB *target = getThreadById(thread_id);
    if (target == NULL) return -3;
    if (target->tid != thread_id) printf("ERROR GETTING CORRECT THREAD\n");
    if (target->observer) {
        if (DEBUG) printf("thread %ld already being joined with\n", thread_id);
        return -2;
    }

    if (target->status == DONE)
    {
        // target thread is already done, just yield
        thread_yield();
        return 0;
    }

    current_thread->status = WAITING;
    target->observer = current_thread;
    if (detect_deadlock()) {
        if (DEBUG) printf("Deadlock detected, no joining\n");
        current_thread->status = WORKING;
        target->observer = NULL;
        return -1;
    }
    thread_yield();
    
    return 0;
}

/// ==== MUTEXING ====

int thread_mutex_init(thread_mutex_t *mutex)
{
    if (!ready) init();
    if (!mutex) return 0;
    mutex->locked = 0;
    mutex->owner = -1;
    if (init_queue(&mutex->q))
    {
        fprintf(stderr, "failed to init a queue\n");
        exit(-1);
    }
    push(mutexes, mutex);
    return 1;
}

int thread_mutex_lock(thread_mutex_t *mutex)
{
    if (!mutex) return 0;
    if (!mutex->locked)
    {
        // acquire the mutex
        mutex->locked = 1;
        mutex->owner = thread_self();
        if (DEBUG) printf("thread %ld locked a mutex\n", current_thread->tid);
        return 1;
    }
    if (mutex->owner == thread_self()) return 0;
    // mutex is locked and not owned by me
    // wait for lock to be given to me
    if (DEBUG) printf("thread %ld waiting for a mutex\n", current_thread->tid);
    current_thread->status = WAITING;
    push(mutex->q, current_thread);
    thread_yield();
    // when control is passed back to this thread,
    // the lock will already be acquired and modified correctly
    return 1;
}

int thread_mutex_unlock(thread_mutex_t *mutex)
{
    if (!mutex) return 0;
    if (!mutex->locked) return 0;
    if (mutex->owner != thread_self()) return 0;
    // this thread owns the mutex and it is locked
    struct TCB *next = pop(mutex->q);
    if (next == NULL)
    {
        if (DEBUG) printf("thread %ld unlocking a mutex\n", current_thread->tid);
        // no waiting thread; unlock
        mutex->locked = 0;
    }
    else
    {
        if (DEBUG) printf("thread %ld giving mutex to thread %ld\n", current_thread->tid, next->tid);
        next->status = WORKING;
        mutex->owner = next->tid;
        push(ready_list, next);
    }
    return 1;
}


/// ==== CLEANUP ==== ///

static void destroy_thread(void *to_free)
{
    if (!to_free) return;
    // if (to_free == current_thread)
    // {
    //     if (DEBUG) printf("trying to destroy current thread! deferring\n");
    //     return;
    // }
    struct TCB *tcb = to_free;
    if (!tcb) return;
    if (DEBUG) printf("freeing thread %ld\n", tcb->tid);
    removeFromQueue(tcb->tid);
    if (DEBUG) printf("removed %ld from queue\n", tcb->tid);
    if (tcb->tid != 0 && tcb->stack) free(tcb->stack);
    free(tcb);
}

static void thread_terminate(void)
{
    if (DEBUG) printf("%ld DONE", current_thread->tid);
    current_thread->status = DONE;
    // wake up the thread waiting on this thread
    if (current_thread->observer)
    {
        if (DEBUG) printf(" waking %ld", current_thread->observer->tid);
        current_thread->observer->status = WORKING;
        push(ready_list, current_thread->observer);
        current_thread->observer = NULL;
    }
    if (DEBUG) printf("\n");
    destroy_thread(prev);
    prev = current_thread;
    thread_yield();
}

// begin thread cleanup process
// destroys all threads and internal data structurs
void thread_cleanup(void)
{
    if (!ready) return;
    // ASSERTION: parent thread is the only thread that can call thread_cleanup
    if (current_thread->tid != 0)
    {
        fprintf(stderr, "Error: thread_cleanup() called from thread other than parent.\n");
        return;
    }

    while (not_empty(all_threads))
    {
        current_thread = pop(all_threads);
        destroy_thread(current_thread);
    }
    thread_mutex_t *mutex = NULL;
    while ((mutex = pop(mutexes)))
    {
        destroy_queue(mutex->q);
        //free(mutex);
    }
    destroy_queue(all_threads);
    destroy_queue(ready_list);
    destroy_queue(mutexes);
    ready = 0;
}