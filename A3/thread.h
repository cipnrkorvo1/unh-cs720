#include <inttypes.h>

// interface for threads library
//

enum Status {
    WORKING,
    WAITING,
    DONE
};

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
    void *handle;           // handle (for management)
} tcb_t;

// PART 1
long thread_create(void (*)(void*), void*);
void thread_yield(void);

void thread_cleanup(void);

// PART 2
long thread_self(void);
int thread_join(long);

typedef struct MUTEX {
    long owner;
    void *q;
    int locked;
} thread_mutex_t;

int thread_mutex_init(thread_mutex_t *mutex);
int thread_mutex_lock(thread_mutex_t *mutex);
int thread_mutex_unlock(thread_mutex_t *mutex);

// PART 3
typedef struct COND {
    void *q;
} thread_cond_t;

int thread_cond_init(thread_cond_t *cond);
int thread_cond_wait(thread_cond_t *cond, thread_mutex_t *mutex);
int thread_cond_signal(thread_cond_t *cond);