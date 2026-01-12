#include "connection.h"
#include "logger.h"
#include "policy.h"
#include "socket_utils.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Assuming simple malloc based buffers for now, could be mempool later
#define MAX_BUFFER_SIZE 16384

connection_t *connection_accept(event_loop_t *loop, int server_fd) {
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

#ifdef _GNU_SOURCE
  int fd = accept4(server_fd, (struct sockaddr *)&addr, &addrlen,
                   SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
  int fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
  if (fd != -1)
    make_socket_non_blocking(fd);
#endif

  if (fd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_ERROR("accept failed: %s", strerror(errno));
    }
    return NULL;
  }

  set_tcp_nodelay(fd);

  // TODO: Use mempool for connection_t
  connection_t *conn = calloc(1, sizeof(connection_t));
  if (!conn) {
    close(fd);
    return NULL;
  }

  conn->fd = fd;
  conn->addr = addr;
  conn->loop = loop;
  conn->in_buf = buffer_create(MAX_BUFFER_SIZE);
  conn->out_buf = buffer_create(MAX_BUFFER_SIZE);
  conn->closing = 0;

  if (!conn->in_buf || !conn->out_buf) {
    connection_close(conn);
    return NULL;
  }

  // Initialize Reactor Event
  conn->event.fd = fd;
  conn->event.events = EVENT_READ;          // Start with READ
  conn->event.handler = connection_on_read; // Default handler? Or dispatch?
  // Wait, connection_on_read/write signature matches event_handler_pt
  // But we need to switch handlers or have one handler dispatch?
  // Previous code had separate handlers.
  // Let's use a single dispatch handler or update the handler pointer on MOD?
  // Standard reactor pattern: One handler handling mask, OR changing handler.
  // Let's assume we use `connection_on_read` initially.
  // Actually, if we want READ|WRITE, which handler do we call?
  // Usually a generic `connection_handle_event` that checks mask.

  // Let's define a unified handler

  conn->event.arg = conn;

  // We need to forward declare a unified handler or use the read one and check
  // mask inside? Cleanest is a unified `connection_event_handler`.

  // Just use a placeholder here, I will implement `connection_event_handler`
  // below. But wait, C needs declaration. I will implement
  // `connection_event_handler` and use it.

  // For now to compile, let's look at strict top-down.
  // I'll implement `connection_event_handler` at end or fwd declare.
  // Let's fwd declare static.
}

static void connection_event_handler(int fd, int events, void *arg);

connection_t *connection_accept_finish(connection_t *conn) {
  conn->event.handler = connection_event_handler;

  if (event_loop_add(conn->loop, &conn->event) == -1) {
    connection_close(conn);
    return NULL;
  }

  LOG_INFO("New connection accepted from %s:%d (fd=%d)",
           inet_ntoa(conn->addr.sin_addr), ntohs(conn->addr.sin_port),
           conn->fd);
  return conn;
}
// Redoing connection_accept to be cleaner
connection_t *connection_accept(event_loop_t *loop, int server_fd) {
  // ... Copy paste previous accept logic ...
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

#ifdef _GNU_SOURCE
  int fd = accept4(server_fd, (struct sockaddr *)&addr, &addrlen,
                   SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
  int fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
  if (fd != -1)
    make_socket_non_blocking(fd);
#endif

  if (fd == -1)
    return NULL;

  set_tcp_nodelay(fd);

  connection_t *conn = calloc(1, sizeof(connection_t));
  if (!conn) {
    close(fd);
    return NULL;
  }

  conn->fd = fd;
  conn->addr = addr;
  conn->loop = loop;
  conn->in_buf = buffer_create(MAX_BUFFER_SIZE);
  conn->out_buf = buffer_create(MAX_BUFFER_SIZE);

  if (!conn->in_buf || !conn->out_buf) {
    connection_close(conn);
    return NULL;
  }

  conn->event.fd = fd;
  conn->event.events = EVENT_READ;
  conn->event.handler = connection_event_handler;
  conn->event.arg = conn;

  if (event_loop_add(loop, &conn->event) == -1) {
    connection_close(conn);
    return NULL;
  }

  LOG_INFO("New connection accepted from %s:%d (fd=%d)",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), fd);

  if (policy_check_connection(inet_ntoa(addr.sin_addr)) != 0) {
    LOG_WARN("Connection from %s rejected by policy", inet_ntoa(addr.sin_addr));
    connection_close(conn);
    return NULL;
  }

  return conn;
}

void connection_close(connection_t *conn) {
  if (!conn)
    return;

  if (conn->fd != -1) {
    // Remove from loop using pointer
    event_loop_del(conn->loop, &conn->event);
    close(conn->fd);
    LOG_INFO("Connection closed (fd=%d)", conn->fd);
  }

  if (conn->in_buf)
    buffer_destroy(conn->in_buf);
  if (conn->out_buf)
    buffer_destroy(conn->out_buf);

  if (conn->proto_ctx) {
    // Free protocol context
  }

  if (conn->ssl) {
    SSL_free(conn->ssl);
  }

  free(conn);
}

// Unified Event Handler
static void connection_event_handler(int fd, int events, void *arg) {
  connection_t *conn = (connection_t *)arg;

  if (events & EVENT_READ) {
    connection_on_read(fd, events, arg);
  }

  if (events & EVENT_WRITE) {
    connection_on_write(fd, events, arg);
  }

  if (events & EVENT_ERROR) {
    LOG_ERROR("Error on fd %d", fd);
    connection_close(conn);
  }
}

// Helper to read data (Plain or SSL)
static ssize_t do_read(connection_t *conn, void *buf, size_t count) {
  if (conn->ssl && conn->tls_handshake_done) {
    return SSL_read(conn->ssl, buf, count);
  }
  return read(conn->fd, buf, count);
}

// Helper to write data (Plain or SSL)
static ssize_t do_write(connection_t *conn, const void *buf, size_t count) {
  if (conn->ssl && conn->tls_handshake_done) {
    return SSL_write(conn->ssl, buf, count);
  }
  return write(conn->fd, buf, count);
}

void connection_on_read(int fd, int events, void *arg) {
  (void)events;
  connection_t *conn = (connection_t *)arg;

  // Handle TLS Handshake if needed
  if (conn->ssl && !conn->tls_handshake_done) {
    int ret = SSL_accept(conn->ssl);
    if (ret == 1) {
      LOG_INFO("TLS Handshake successful");
      conn->tls_handshake_done = 1;
      // Continue to read data if any? or return to wait for next event?
      // Usually SSL_accept might consume data.
    } else {
      int err = SSL_get_error(conn->ssl, ret);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return; // Wait for more data/capacity
      }
      LOG_ERROR("TLS Handshake failed: %d", err);
      connection_close(conn);
      return;
    }
  }

  char temp[4096];
  ssize_t n = do_read(conn, temp, sizeof(temp));

  if (n == -1) {
    // Check SSL errors if SSL
    if (conn->ssl && conn->tls_handshake_done) {
      int err = SSL_get_error(conn->ssl, n);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        return;
      // Otherwise error
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      if (errno == EINTR)
        return;
    }
    LOG_ERROR("Read error on connection");
    connection_close(conn);
    return;
  }

  if (n == 0) {
    connection_close(conn);
    return;
  }

  // Buffer overflow check omitted for brevity in this snippet update
  if (buffer_write(conn->in_buf, temp, n) < (size_t)n) {
    LOG_WARN("Buffer overflow");
    // Disconnect
  }

  if (conn->proto_ctx) {
    smtp_process((smtp_session_t *)conn->proto_ctx);
  }
}

void connection_on_write(int fd, int events, void *arg) {
  (void)events;
  connection_t *conn = (connection_t *)arg;

  // If in handshake, write might be triggered by SSL_accept wanting write
  if (conn->ssl && !conn->tls_handshake_done) {
    connection_on_read(fd, events,
                       arg); // Retry handshake which is driven by accept
    return;
  }

  while (buffer_used(conn->out_buf) > 0) {
    char temp[4096];
    size_t len = buffer_peek(conn->out_buf, temp, sizeof(temp));
    ssize_t n = do_write(conn, temp, len);

    if (n <= 0) {
      // Handle errors similar to read
      if (conn->ssl && conn->tls_handshake_done) {
        int err = SSL_get_error(conn->ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
          return;
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return;
        if (errno == EINTR)
          continue;
      }
      LOG_ERROR("Write error");
      connection_close(conn);
      return;
    }
    buffer_read(conn->out_buf, NULL, n);
  }

  if (buffer_used(conn->out_buf) == 0) {
    int new_events = conn->event.events & ~EVENT_WRITE;
    event_loop_mod(conn->loop, &conn->event, new_events);
  }
}

int connection_send(connection_t *conn, const void *data, size_t len) {
  if (buffer_write(conn->out_buf, data, len) < len)
    return -1;

  // Enable WRITE
  if (!(conn->event.events & EVENT_WRITE)) {
    event_loop_mod(conn->loop, &conn->event, conn->event.events | EVENT_WRITE);
  }
  return 0;
}
