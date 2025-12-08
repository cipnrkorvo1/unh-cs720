#include "queue.h"

typedef struct QueueItem {
    void *data;
    struct QueueItem *next;
} qitem_t;

struct Queue {
    qitem_t *front;
    qitem_t *back;
};

/**
 * Initializes the queue at the given address and returns it.
 * Return 0 for success, 1 for failure.
 */
int init_queue(void **Q)
{
    struct Queue *queue = malloc(sizeof(struct Queue));
    if (queue == NULL) return 1;
    queue->front = NULL;
    queue->back = NULL;
    *Q = queue;
    return 0;
}

int queue_not_empty(void *Q)
{
    struct Queue *queue = Q;
    if (!queue) return 0;
    return queue->front != NULL;
}

/**
 * Creates an Item with the given data and adds it to the back of the queue.
 */
void queue_push(void *Q, void *data)
{
    struct Queue *queue = Q;
    qitem_t *item = malloc(sizeof(qitem_t));
    item->data = data;
    item->next = NULL;
    if (queue->back)
    {
        queue->back->next = item;
        queue->back = item;
    }
    else 
    {
        queue->front = item;
        queue->back = item;
    }
}

/**
 * Returns the value of the frontmost Item without popping it.
 * Returns NULL if queue is empty or invalid.
 */
void* queue_peek(void *Q)
{
    struct Queue *queue = Q;
    if (!queue || queue->front == NULL)
    {
        return NULL;
    }
    return queue->front;
}

/**
 * Removes the front Item from the queue and returns its data.
 */
void* queue_pop(void *Q)
{
    struct Queue *queue = Q;
    if (queue->front == NULL)
    {
        return NULL;
    }
    qitem_t *item = queue->front;
    queue->front = item->next;
    if (queue->front == NULL)
    {
        queue->back = NULL;
    }
    void *data = item->data;
    free(item);
    return data;

}

/**
 * Returns the first qitem from the queue, NOT the data.
 * Can then be used as an iterator.
 */
void* iterator(void *Q)
{
    return ((struct Queue*)Q)->front;
}

/**
 * Returns the data from the current iterator AND updates the iterator.
 */
void* next(void **I)
{
    if (*I == NULL) return NULL;
    qitem_t *item = *I;
    *I = ((qitem_t*)*I)->next;
    return item->data;
}

int queue_remove(void *Q, void *data)
{
    struct Queue *queue = Q;
    if (!queue || !data) return 1;
    qitem_t *prev = queue->front;
    qitem_t *item = queue->front;
    if (!item) return 1;
    while (item) {
        if (item->data == data)
        {
            if (item == queue->front)
            {
                queue->front = item->next;
            }
            else
            {
                prev->next = item->next;
            }
            free(item);
            return 0;
        }
        prev = item;
        item = item->next;
    }
    return 1;
}

/**
 * Frees memory associated with the queue.
 */
void destroy_queue(void *Q)
{
    struct Queue *queue = Q;
    if (!queue) return;
    while (queue->front)
    {
        qitem_t *item = queue->front;
        queue->front = item->next;
        free(item);
    }
    free(queue);
}
