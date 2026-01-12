#include "reactor.h"
#include "logger.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 1024

struct event_loop {
  int epoll_fd;
  struct epoll_event events[MAX_EVENTS];
  int stop;
};

event_loop_t *event_loop_create(int size) {
  (void)size;
  event_loop_t *loop = calloc(1, sizeof(event_loop_t));
  if (!loop)
    return NULL;

  loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (loop->epoll_fd == -1) {
    LOG_FATAL("epoll_create1 failed: %s", strerror(errno));
    free(loop);
    return NULL;
  }

  loop->stop = 0;
  return loop;
}

void event_loop_destroy(event_loop_t *loop) {
  if (loop) {
    close(loop->epoll_fd);
    free(loop);
  }
}

int event_loop_add(event_loop_t *loop, reactor_event_t *event) {
  struct epoll_event ev;
  ev.events = EPOLLET;
  if (event->events & EVENT_READ)
    ev.events |= EPOLLIN;
  if (event->events & EVENT_WRITE)
    ev.events |= EPOLLOUT;
  ev.data.ptr = event;

  if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, event->fd, &ev) == -1) {
    LOG_ERROR("epoll_ctl ADD failed for fd %d: %s", event->fd, strerror(errno));
    return -1;
  }
  return 0;
}

int event_loop_del(event_loop_t *loop, reactor_event_t *event) {
  // In Linux < 2.6.9, event must be non-NULL, but we use modern kernel
  // Pass NULL is usually fine IF supported, but safer to pass a dummy or the
  // event ptr itself. Standard says for DEL, the event arg is ignored, but
  // purely for portability:
  if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, event->fd, NULL) == -1) {
    // Treat ENOENT as success (already removed/closed)
    if (errno != ENOENT) {
      LOG_ERROR("epoll_ctl DEL failed for fd %d: %s", event->fd,
                strerror(errno));
      return -1;
    }
  }
  return 0;
}

int event_loop_mod(event_loop_t *loop, reactor_event_t *event, int new_events) {
  struct epoll_event ev;
  ev.events = EPOLLET;
  if (new_events & EVENT_READ)
    ev.events |= EPOLLIN;
  if (new_events & EVENT_WRITE)
    ev.events |= EPOLLOUT;
  ev.data.ptr = event;

  // Update the event structure itself
  event->events = new_events;

  if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, event->fd, &ev) == -1) {
    LOG_ERROR("epoll_ctl MOD failed for fd %d: %s", event->fd, strerror(errno));
    return -1;
  }
  return 0;
}

void event_loop_run(event_loop_t *loop) {
  while (!loop->stop) {
    int nfds = epoll_wait(loop->epoll_fd, loop->events, MAX_EVENTS, 100);

    if (nfds == -1) {
      if (errno == EINTR)
        continue;
      LOG_FATAL("epoll_wait failed: %s", strerror(errno));
      return;
    }

    for (int i = 0; i < nfds; i++) {
      reactor_event_t *event = (reactor_event_t *)loop->events[i].data.ptr;
      int native_events = loop->events[i].events;
      int mask = 0;

      if (native_events & EPOLLIN)
        mask |= EVENT_READ;
      if (native_events & EPOLLOUT)
        mask |= EVENT_WRITE;
      if (native_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        mask |= EVENT_ERROR;

      if (event && event->handler) {
        event->handler(event->fd, mask, event->arg);
      }
    }
  }
}

void event_loop_stop(event_loop_t *loop) { loop->stop = 1; }
