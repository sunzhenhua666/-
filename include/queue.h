#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct queue_node {
  void *data;
  struct queue_node *next;
} queue_node_t;

typedef struct {
  queue_node_t *head;
  queue_node_t *tail;
  int count;
  int stop;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

queue_t *queue_create(void);
void queue_destroy(queue_t *q);
void queue_push(queue_t *q, void *data);
void *queue_pop(queue_t *q);    // Blocking
void queue_stop(queue_t *q);    // Unblock all pops
int queue_is_empty(queue_t *q); // Check if queue is empty

#endif // QUEUE_H
