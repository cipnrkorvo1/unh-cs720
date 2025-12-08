#include "thread.h"

typedef struct TCB_HANDLE {
    tcb_t *tcb;
    unsigned long id;
    void *next;
} thandle_t;

struct TABLE {
    thandle_t *head;
    unsigned long max_id;
    int size;
};

// init table
// return 0 for success, -1 for failure
int init_table(void **T);

// put tcb into table
// return 0 for success or -1 for invalid pointers (failure)
int put_tcb(struct TABLE *table, tcb_t *tcb);

// get tcb by (table)id from table
void *get_tcb_by_id(struct TABLE *table, unsigned long target_id);

// get tcb by tcb reference from table
void *get_tcb(struct TABLE *table, tcb_t *tcb);

// removes the top item from the table and returns it, or NULL if empty
void *pop_tcb(struct TABLE *table);

// remove tcb from table
// return 0 for success, -1 for invalid pointers, -2 for item not found
int remove_tcb(struct TABLE *table, tcb_t *tcb);

int destroy_table(struct TABLE *table);
