#ifndef CONNECTION_H
#define CONNECTION_H

#include "buffer.h"
#include "reactor.h"
#include <netinet/in.h>
#include <openssl/ssl.h> // Added for SSL types

// Forward declaration for SMTP context (void* for now to break
// dependency)
typedef struct smtp_session smtp_session_t;

typedef struct connection {
  int fd;
  struct sockaddr_in addr;

  buffer_t *in_buf;
  buffer_t *out_buf;

  // Reactor loop reference
  event_loop_t *loop;

  // Embedded reactor event (Caller-owned context for epoll)
  reactor_event_t event;

  // TLS State
  SSL *ssl;
  int tls_enabled;
  int tls_handshake_done;

  // Protocol Context (SMTP Session)
  void *proto_ctx;

  // Flags
  int closing;
} connection_t;

// Accept a new connection
connection_t *connection_accept(event_loop_t *loop, int server_fd);

// Close connection
void connection_close(connection_t *conn);

// Read data (called by reactor)
void connection_on_read(int fd, int events, void *arg);

// Write data (called by reactor)
void connection_on_write(int fd, int events, void *arg);

// Send data (queues to out_buf, registers write event if needed)
int connection_send(connection_t *conn, const void *data, size_t len);

// TLS Upgrade
int connection_start_tls(connection_t *conn, SSL_CTX *ctx);

#endif // CONNECTION_H
