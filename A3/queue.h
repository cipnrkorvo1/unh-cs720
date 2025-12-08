#include <stdlib.h>

// QUEUE
/**
 * Initializes the queue at the given address and returns it.
 * Return 0 for success, 1 for failure.
 */
int init_queue(void **Q);

int queue_not_empty(void *Q);

/**
 * Creates an Item with the given data and adds it to the back of the queue.
 */
void queue_push(void *Q, void *data);

/**
 * Returns the value of the frontmost Item without popping it.
 * Returns NULL if queue is empty or invalid.
 */
void* queue_peek(void *Q);

/**
 * Removes the front Item from the queue and returns its data.
 */
void* queue_pop(void *Q);

/**
 * Returns the first qitem from the queue, NOT the data.
 * Can then be used as an iterator.
 */
void* iterator(void *Q);

/**
 * Returns the data from the current iterator AND updates the iterator.
 */
void* next(void **I);

int queue_remove(void *Q, void *data);

/**
 * Frees memory associated with the queue.
 */
void destroy_queue(void *Q);