#include "queue.h"
#include <stdlib.h>

queue_t *queue_create(void) {
  queue_t *q = calloc(1, sizeof(queue_t));
  if (!q)
    return NULL;

  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
  return q;
}

void queue_destroy(queue_t *q) {
  if (!q)
    return;
  queue_stop(q);
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->cond);

  // Free remaining nodes
  queue_node_t *current = q->head;
  while (current) {
    queue_node_t *next = current->next;
    free(current);
    current = next;
  }
  free(q);
}

void queue_push(queue_t *q, void *data) {
  queue_node_t *node = malloc(sizeof(queue_node_t));
  if (!node)
    return;
  node->data = data;
  node->next = NULL;

  pthread_mutex_lock(&q->mutex);
  if (q->tail) {
    q->tail->next = node;
    q->tail = node;
  } else {
    q->head = q->tail = node;
  }
  q->count++;
  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

void *queue_pop(queue_t *q) {
  pthread_mutex_lock(&q->mutex);
  while (q->head == NULL && !q->stop) {
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  if (q->stop && q->head == NULL) {
    pthread_mutex_unlock(&q->mutex);
    return NULL;
  }

  queue_node_t *node = q->head;
  void *data = node->data;
  q->head = node->next;
  if (q->head == NULL) {
    q->tail = NULL;
  }
  q->count--;
  pthread_mutex_unlock(&q->mutex);

  free(node);
  return data;
}

void queue_stop(queue_t *q) {
  pthread_mutex_lock(&q->mutex);
  q->stop = 1;
  pthread_cond_broadcast(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

int queue_is_empty(queue_t *q) {
  pthread_mutex_lock(&q->mutex);
  int empty = (q->head == NULL);
  pthread_mutex_unlock(&q->mutex);
  return empty;
}
