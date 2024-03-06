#include "queue.h"

#include <assert.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

typedef struct queue {
    void **queue;
    int in, out;
    int size;
    sem_t empty, full;
    pthread_mutex_t mutex;
} queue;

queue_t *queue_new(int size) {
    queue *t = calloc(1, sizeof(queue));
    t->in = t->out = 0;
    t->size = size;
    int rc = 0;
    // sem_t test;
    rc = sem_init(&t->empty, 0, size);
    assert(!rc);
    rc = sem_init(&t->full, 0, 0);
    assert(!rc);
    rc = pthread_mutex_init(&(t->mutex), NULL);
    assert(!rc);
    t->queue = calloc(size, sizeof(void *));
    return t;
}

void queue_delete(queue_t **q) {
    // int size = (*q)->empty + *q->full;
    sem_destroy(&((*q)->empty));
    sem_destroy(&((*q)->full));
    pthread_mutex_destroy(&((*q)->mutex));
    free((*q)->queue);
    *(*q)->queue = NULL;
    free(*q);
    *q = NULL;
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) {
        return false;
    }
    sem_wait(&(q->empty));
    pthread_mutex_lock(&(q->mutex));
    q->queue[q->in] = elem;
    q->in = (q->in + 1) % q->size;
    pthread_mutex_unlock(&(q->mutex));
    sem_post(&(q->full));
    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    }
    sem_wait(&(q->full));
    pthread_mutex_lock(&(q->mutex));
    *elem = q->queue[q->out];
    q->out = (q->out + 1) % q->size;
    pthread_mutex_unlock(&(q->mutex));
    sem_post(&(q->empty));
    return true;
}
