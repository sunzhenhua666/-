#ifndef LIST_H
#define LIST_H

#include <stddef.h>

// Intrusive List Node
typedef struct list_node {
  struct list_node *prev;
  struct list_node *next;
} list_node_t;

// List Head
typedef struct {
  list_node_t *head;
  list_node_t *tail;
  size_t size;
} list_t;

// Init list
void list_init(list_t *list);

// Append to tail
void list_push_back(list_t *list, list_node_t *node);

// Prepend to head
void list_push_front(list_t *list, list_node_t *node);

// Remove node
void list_remove(list_t *list, list_node_t *node);

// Pop head
list_node_t *list_pop_front(list_t *list);

// Get size
size_t list_size(list_t *list);

#define list_entry(ptr, type, member)                                          \
  ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define list_for_each(pos, list)                                               \
  for (pos = (list)->head; pos != NULL; pos = pos->next)

#define list_for_each_safe(pos, n, list)                                       \
  for (pos = (list)->head, n = pos ? pos->next : NULL; pos != NULL;            \
       pos = n, n = pos ? pos->next : NULL)

#endif // LIST_H
