#ifndef SMTP_SERVER_H
#define SMTP_SERVER_H

#include "connection.h"
#include "mempool.h"
#include "storage.h"
#include <openssl/ssl.h>

// Forward declaration to avoid circular dependency
struct connection;

// SMTP States
typedef enum {
  SMTP_STATE_CONNECT,
  SMTP_STATE_GREETING,
  SMTP_STATE_HELO,
  SMTP_STATE_MAIL,
  SMTP_STATE_RCPT,
  SMTP_STATE_DATA,
  SMTP_STATE_DATA_CONTENT,
  SMTP_STATE_QUIT,
  SMTP_STATE_ERROR
} smtp_state_t;

// SMTP Envelope (Sender, Recipients)
typedef struct {
  char *sender;      // Allocated from mempool
  char **recipients; // Array of pointers (mempool)
  int recipient_count;
  int recipient_capacity;
} smtp_envelope_t;

// SMTP Session Context
typedef struct smtp_session {
  connection_t *conn;
  mempool_t *pool;          // Session-bound memory pool
  storage_ctx_t *store_ctx; // Storage transaction handle

  smtp_state_t state;
  smtp_envelope_t env;

  // Command buffer for line-based processing
  char cmd_buffer[1024];

  // Flags
  int is_esmtp;
} smtp_session_t;

// Set global SSL context for SMTP server
void smtp_server_set_ssl_ctx(SSL_CTX *ctx);

// Create a new SMTP session attached to a connection
void smtp_session_init(smtp_session_t *session, connection_t *conn);

// Destroy session
void smtp_session_destroy(smtp_session_t *session);

// Process incoming data (called by connection layer)
void smtp_process(smtp_session_t *session);

#endif // SMTP_SERVER_H
