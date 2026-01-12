#include "list.h"

void list_init(list_t *list) {
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
}

void list_push_back(list_t *list, list_node_t *node) {
  node->next = NULL;
  node->prev = list->tail;

  if (list->tail) {
    list->tail->next = node;
  } else {
    list->head = node;
  }
  list->tail = node;
  list->size++;
}

void list_push_front(list_t *list, list_node_t *node) {
  node->next = list->head;
  node->prev = NULL;

  if (list->head) {
    list->head->prev = node;
  } else {
    list->tail = node;
  }
  list->head = node;
  list->size++;
}

void list_remove(list_t *list, list_node_t *node) {
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    list->head = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  } else {
    list->tail = node->prev;
  }

  node->next = NULL;
  node->prev = NULL;
  list->size--;
}

list_node_t *list_pop_front(list_t *list) {
  if (!list->head)
    return NULL;

  list_node_t *node = list->head;
  list_remove(list, node);
  return node;
}

size_t list_size(list_t *list) { return list->size; }
