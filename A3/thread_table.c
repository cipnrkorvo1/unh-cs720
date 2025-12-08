#include <stdio.h>
#include <stdlib.h>

#include "thread_table.h"

#ifndef DEBUG
    #define DEBUG 0
#endif

static int malloc_fail()
{
    fprintf(stderr, "malloc fail\n");
    exit(-1);
}

int init_table(void **T)
{
    if (T == NULL) return -1;
    struct TABLE *table = malloc(sizeof(struct TABLE));
    if (table == NULL) malloc_fail();
    table->head = NULL;
    table->max_id = 0;
    table->size = 0;
    *T = table;
    if (DEBUG) printf("table initialized\n");
    return 0;
}

// put tcb into table
// return 0 for success or -1 for invalid pointers (failure)
int put_tcb(struct TABLE *table, tcb_t *tcb)
{
    if (!table || !tcb)
    {
        return -1;
    }
    
    thandle_t *handle = malloc(sizeof(thandle_t));
    if (!handle) malloc_fail();
    handle->tcb = tcb;
    handle->id = table->max_id++;
    tcb->tid = handle->id;      // set tcb->tid
    // set new handle as head of table (stack)
    handle->next = table->head;
    table->head = handle;
    table->size++;

    // replace tcb handle (free old handle if applicable (probably not))
    if (tcb->handle) free(tcb->handle);
    tcb->handle = handle;
    if (DEBUG) printf("table put %lu\n", tcb->tid);

    return 0;
}

// get tcb by (table)id from table
void *get_tcb_by_id(struct TABLE *table, unsigned long target_id)
{
    if (DEBUG) printf("table get %lu\n", target_id);
    // search for target
    // (stack should be sorted by -id; stop searching when encountering id less than target)
    thandle_t *ptr = table->head;
    while (ptr && ptr->id > target_id)
    {
        ptr = ptr->next;
    }
    // check if found
    if (!ptr) return NULL;
    if (ptr->id != target_id) return NULL;
    return ptr->tcb;
}

// get tcb by tcb reference from table
void *get_tcb(struct TABLE *table, tcb_t *tcb)
{
    if (!table || !tcb || !tcb->handle) return NULL;
    return get_tcb_by_id(table, ((thandle_t *)(tcb->handle))->id);
}

// removes the top item from the table and returns it, or NULL if empty
void *pop_tcb(struct TABLE *table)
{
    if (!table || !table->head) return NULL;
    thandle_t *ret = table->head;
    table->head = table->head->next;
    ret->tcb->handle = NULL;
    table->size--;
    tcb_t *tcb = ret->tcb;
    free(ret);
    if (DEBUG) printf("table pop %lu\n", tcb->tid);
    return tcb;
}

// remove tcb from table
// return 0 for success, -1 for invalid pointers, -2 for item not found
int remove_tcb(struct TABLE *table, tcb_t *tcb)
{
    if (!table || !tcb || !tcb->handle) return -1;
    unsigned long target = ((thandle_t *)(tcb->handle))->id;
    if (DEBUG) printf("table remove %lu\n", target);
    // search for target
    // (stack should be sorted by -id; stop searching when encountering id less than target)
    thandle_t *prev = NULL;
    thandle_t *ptr = table->head;
    while (ptr && ptr->id > target)
    {
        prev = ptr;
        ptr = ptr->next;
    }
    // check if found
    if (!ptr) return -2;
    if (ptr->id != target) return -2;
    // found, replace
    tcb->handle = NULL;    // remove tcb reference to handle
    if (!prev) {
        // tcb was table->head
        table->head = ptr->next;
    } else {
        prev->next = ptr->next;
    }
    free(ptr);                  // free mem allocated to that handle
    table->size--;
    return 0;
}

int destroy_table(struct TABLE *table)
{
    if (!table) return -1;
    if (DEBUG) printf("destroy table\n");
    while (table->head)
    {
        thandle_t *temp = table->head->next;
        if (table->head->tcb) table->head->tcb->handle = NULL;
        free(table->head);
        table->head = temp;
    }
    free(table);
    return 0;
}

/*
int main()
{
    void *table;
    if (init_table(&table)) {
        fprintf(stderr, "failed to init table\n");
        return -1;
    }
    tcb_t tcb = {{1,2,3}, NULL};
    tcb_t junk1 = {{4,5,6}, NULL};
    tcb_t junk2 = {{7,8,9}, NULL};
    if (put_tcb(table, &tcb) || put_tcb(table, &junk1) || put_tcb(table, &junk2)) {
        fprintf(stderr, "failed to put\n");
        return -1;
    }
    if (!tcb.handle)
    {
        fprintf(stderr, "tcb does not have handle ref\n");
        return -1;
    }
    tcb_t *ptr = NULL;
    if (!(ptr = get_tcb(table, &tcb))) {
        fprintf(stderr, "failed to get\n");
        return -1;
    }
    if (ptr != &tcb)
    {
        fprintf(stderr, "get returned different ptr\n");
        return -1;
    }
    int res;
    if ((res = remove_tcb(table, &tcb)))
    {
        fprintf(stderr, "remove fail %d\n", res);
        return -1;
    }
    if (tcb.handle)
    {
        fprintf(stderr, "tcb still has handle\n");
        return -1;
    }
    if (get_tcb(table, &tcb))
    {
        fprintf(stderr, "get after remove succeeded\n");
        return -1;
    }
    destroy_table(table);
    table = NULL;
    return 0;
}
    */