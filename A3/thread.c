#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "thread_table.h"
#include "queue.h"

#define STACK_SIZE 65536

#define DEBUG 0

/**
 * x86-64 passes discrete arugments in registers %rdi, %rsi, %rdx, %rcx, %r8, %r9
 * Return value is expected to be in %rax
 * Linux kernel is expected to pass the syscall number through %rax
 */

extern void asm_yield(void *current, void *next);

static int ready = 0;
void *all_threads = NULL;   // thread_table
void *ready_list = NULL;    // queue
void *mutexes = NULL;       // queue
void *conds = NULL;         // queue
void *next_to_destroy = NULL;   // for deferring destruction of threads
static unsigned long parent_id;

static tcb_t *current_thread = NULL; // TCB

static void thread_terminate(void); // function to run when thread finishes
static void destroy_thread(void *); // function to free thread

/// ==== INIT ====

/*static int __num_threads = -1;
static int next_tid()
{
    __num_threads += 1;
    return __num_threads;
}*/

static void init()
{
    if (DEBUG) printf("init()\n");
    if (init_queue(&ready_list) || init_table(&all_threads) || 
            init_queue(&mutexes) || init_queue(&conds))
    {
        fprintf(stderr, "Error initializing a data structure\n");
        exit(-1);
    }
    tcb_t *parent = malloc(sizeof(tcb_t));
    //__num_threads = -1;
    parent->status = WORKING;
    parent->stack = NULL;
    put_tcb(all_threads, parent);   // sets parent->tid
    queue_push(ready_list, parent);
    // set parent as current thread, assuming list was empty
    current_thread = queue_pop(ready_list);

    parent_id = parent->tid;
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

    tcb_t *thread = malloc(sizeof(tcb_t));
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

    put_tcb(all_threads, thread);   // sets thread->tid
    queue_push(ready_list, thread);
    if (DEBUG) printf("created thread %ld\n", thread->tid);
    return thread->tid;
}

/// ==== THREAD YIELD ====

void thread_yield(void)
{
    if (DEBUG) printf("%ld: thread_yield()\n", current_thread->tid);
    if (!ready) init();

    tcb_t* old = current_thread;         // save current thread
    tcb_t* next = queue_pop(ready_list);       // move next thread up
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
        queue_push(ready_list, old);              // push old thread to ready_list
    }
    asm_yield(old, current_thread);     // context switch to new thread 
}

/// ==== THREAD JOIN ==== ///
/**
 * Used by thread_join() to detect if a deadlock has occurred.
 * i.e. checks for cycles in the linked list of observers from current_thread
 */
static int detect_deadlock()
{
    tcb_t *slow = current_thread, *fast = current_thread;
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
    tcb_t *target = get_tcb_by_id(all_threads, thread_id);
    if (target == NULL) return -3;
    if (target->tid != thread_id)
    {
        fprintf(stderr, "FATAL: ERROR GETTING CORRECT THREAD\n");
        exit(-1);
    }
    if (target->status == DONE)
    {
        // target thread is already done, just yield
        thread_yield();
        return 0;
    }
    if (target->observer) {
        if (DEBUG) printf("thread %ld already being joined with\n", thread_id);
        return -2;
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
    queue_push(mutexes, mutex);
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
    queue_push(mutex->q, current_thread);
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
    tcb_t *next = queue_pop(mutex->q);
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
        queue_push(ready_list, next);
    }
    return 1;
}

/// ==== COND SIGNAL ==== ///

struct COND_INFO {
    tcb_t *thread;
    thread_mutex_t *mutex;
};

int thread_cond_init(thread_cond_t *cond)
{
    if (DEBUG) printf("init cond %p\n", cond);
    if (!ready) init();
    if (!cond) return 0;
    // init queue
    if (init_queue(&cond->q))
    {
        fprintf(stderr, "Error initializing a data structure\n");
        exit(-1);
    }
    queue_push(conds, cond);
    return 1;
}

int thread_cond_wait(thread_cond_t *cond, thread_mutex_t *mutex)
{
    // fails if passed a NULL pointer or if the referenced mutex is
    // not locked by the calling thread.
    if (!cond || !mutex) return 0;
    if (!mutex->locked || mutex->owner != current_thread->tid) return 0;
    if (DEBUG) printf("%ld waiting on condition\n", current_thread->tid);
    // release the mutex
    if (!thread_mutex_unlock(mutex)) return 0;
    current_thread->status = WAITING;
    struct COND_INFO *info = malloc(sizeof(struct COND_INFO));
    info->thread = current_thread;
    info->mutex = mutex;
    // block on the condition variable
    queue_push(cond->q, info);
    thread_yield();
    // when control is passed back to this thread,
    //  thread owns the mutex
    return 1;
}

int thread_cond_signal(thread_cond_t *cond)
{
    if (!cond) return 0;
    if (DEBUG) printf("signaling condition %p\n", cond);
    struct COND_INFO *info = queue_pop(cond->q);
    if (!info) return 1;    // no threads waiting!

    // manually contend for mutex
    tcb_t *thread = info->thread;
    thread_mutex_t *mutex = info->mutex;
    if (!mutex->locked)
    {
        // mutex not locked, acquire
        mutex->locked = 1;
        mutex->owner = thread->tid;
        thread->status = WORKING;
        if (DEBUG) printf("thread %ld locked a mutex\n", thread->tid);
        queue_push(ready_list, thread);
    }
    else
    {
        // mutex is locked, set to wait for it
        if (DEBUG) printf("thread %ld waiting on a mutex\n", thread->tid);
        queue_push(mutex->q, thread);
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
    tcb_t *tcb = to_free;
    if (!tcb) return;
    if (DEBUG) printf("freeing thread %ld\n", tcb->tid);
    remove_tcb(all_threads, tcb);
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
        queue_push(ready_list, current_thread->observer);
        current_thread->observer = NULL;
    }
    if (DEBUG) printf("\n");
    destroy_thread(next_to_destroy);
    next_to_destroy = current_thread;
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

    tcb_t *to_delete = pop_tcb(all_threads);
    while (to_delete)
    {
        destroy_thread(to_delete);
        to_delete = pop_tcb(all_threads);
    }
    thread_mutex_t *mutex = NULL;
    while ((mutex = queue_pop(mutexes)))
    {
        destroy_queue(mutex->q);
    }
    thread_cond_t *cond = NULL;
    while ((cond = queue_pop(conds)))
    {
        destroy_queue(cond->q);
    }
    destroy_table(all_threads);
    destroy_queue(ready_list);
    destroy_queue(mutexes);
    destroy_queue(conds);
    ready = 0;
}