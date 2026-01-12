#ifndef REACTOR_H
#define REACTOR_H

#include "list.h"
#include <stdint.h>
#include <time.h>

// Event types
#define EVENT_NONE 0x00
#define EVENT_READ 0x01
#define EVENT_WRITE 0x02
#define EVENT_ERROR 0x04

typedef struct event_loop event_loop_t;

// Event handler callback
typedef void (*event_handler_pt)(int fd, int events, void *arg);

typedef struct reactor_event {
  int fd;
  int events;
  event_handler_pt handler;
  void *arg;
} reactor_event_t;

// Create event loop
event_loop_t *event_loop_create(int size);

// Destroy event loop
void event_loop_destroy(event_loop_t *loop);

// Add event to loop (caller must manage lifecycle of `event` pointer)
int event_loop_add(event_loop_t *loop, reactor_event_t *event);

// Remove event from loop
int event_loop_del(event_loop_t *loop, reactor_event_t *event);

// Update event in loop
int event_loop_mod(event_loop_t *loop, reactor_event_t *event, int new_events);

// Run the loop (blocks)
void event_loop_run(event_loop_t *loop);

// Stop the loop
void event_loop_stop(event_loop_t *loop);

#endif // REACTOR_H
