#include "logger.h"
#include "mempool.h"
#include "policy.h"
#include "smtp_server.h"
#include "tls.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void send_reply(smtp_session_t *s, int code, const char *msg) {
  char buf[512];
  int len = snprintf(buf, sizeof(buf), "%d %s\r\n", code, msg);
  connection_send(s->conn, buf, len);
}

smtp_session_t *smtp_session_create(connection_t *conn) {
  // We can't use mempool for the session struct itself easily unless the conn
  // manages it purely But for simplicity, we malloc session, but use mempool
  // for its components.
  smtp_session_t *s = calloc(1, sizeof(smtp_session_t));
  if (!s)
    return NULL;

  s->conn = conn;

  // Create Session Mempool (start small, grow as needed)
  s->pool = mempool_create(4096);
  if (!s->pool) {
    free(s);
    return NULL;
  }

  // Initialize envelope
  s->env.recipient_capacity = 10;
  s->env.recipients =
      mempool_alloc(s->pool, sizeof(char *) * s->env.recipient_capacity);
  s->env.recipient_count = 0;

  // Initial State
  s->state = SMTP_STATE_CONNECT;

  // Send Greeting
  send_reply(s, 220, "HighPerfSMTP Relay Service Ready");
  s->state = SMTP_STATE_HELO;

  return s;
}

void smtp_session_destroy(smtp_session_t *s) {
  if (s) {
    if (s->store_ctx) {
      storage_abort(s->store_ctx);
    }
    if (s->pool) {
      mempool_destroy(s->pool);
    }
    free(s);
  }
}

// Strip leading/trailing whitespace
static char *trim_whitespace(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

static void process_helo(smtp_session_t *s, char *arg, int is_esmtp) {
  s->is_esmtp = is_esmtp;
  LOG_INFO("Client HELO/EHLO: %s", arg);

  if (is_esmtp) {
    // Send multiline response for EHLO
    const char *resp =
        "250-HighPerfSMTP\r\n250-8BITMIME\r\n250-PIPELINING\r\n250 OK\r\n";
    connection_send(s->conn, resp, strlen(resp));
  } else {
    send_reply(s, 250, "OK");
  }
  s->state = SMTP_STATE_MAIL;

  // Reset Envelope
  s->env.recipient_count = 0;
  s->env.sender = NULL;
}

static void process_mail(smtp_session_t *s, char *arg) {
  // Expect arg: "FROM:<...>"
  // Simple parsing logic: skip "FROM:"
  char *p = strchr(arg, ':');
  if (!p) {
    send_reply(s, 501, "Syntax error in parameters or arguments");
    return;
  }
  p++; // Skip ':'

  // Save sender using mempool
  s->env.sender = mempool_strdup(s->pool, trim_whitespace(p));
  LOG_INFO("MAIL FROM: %s", s->env.sender);

  if (policy_check_sender(s->env.sender) != 0) {
    send_reply(s, 550, "Sender rejected by policy");
    // s->env.sender = NULL; // Should we clear it? state remains MAIL?
    // Usually standard says state stays
    return;
  }

  send_reply(s, 250, "OK");
  s->state = SMTP_STATE_RCPT;
}

static void process_starttls(smtp_session_t *s, char *arg) {
  (void)arg;
  if (!g_ssl_ctx) {
    send_reply(s, 454, "TLS not available due to temporary reason");
    return;
  }

  if (s->conn->ssl) {
    send_reply(s, 454, "TLS already active");
    return;
  }

  send_reply(s, 220, "Ready to start TLS");

  // Upgrade connection
  if (connection_start_tls(s->conn, g_ssl_ctx) != 0) {
    LOG_ERROR("Failed to upgrade connection to TLS");
    // Close?
    smtp_session_destroy(s);
    return;
  }

  // Reset state for new HELO/EHLO
  // Note: State remains connected, but AUTH/EHLO info often flushed.
  // For simplicity, we just reset envelope.
  if (s->env.sender) {
    // In real world, session context is largely reset.
  }
  s->state = SMTP_STATE_INIT; // Back to needing HELO
}

static void process_rcpt(smtp_session_t *s, char *arg) {
  char *p = strchr(arg, ':');
  if (!p) {
    send_reply(s, 501, "Syntax error");
    return;
  }
  p++;

  char *rcpt_val = trim_whitespace(p);
  if (policy_check_recipient(rcpt_val) != 0) {
    send_reply(s, 550, "Recipient rejected by policy");
    return;
  }

  if (s->env.recipient_count >= s->env.recipient_capacity) {
    // In a real mempool impl, realloc is tricky.
    // We'd allocate a new larger array and copy pointers.
    int new_cap = s->env.recipient_capacity * 2;
    char **new_list = mempool_alloc(s->pool, sizeof(char *) * new_cap);
    if (new_list) {
      memcpy(new_list, s->env.recipients,
             sizeof(char *) * s->env.recipient_count);
      s->env.recipients = new_list;
      s->env.recipient_capacity = new_cap;
    } else {
      send_reply(s, 452, "Too many recipients");
      return;
    }
  }

  s->env.recipients[s->env.recipient_count++] =
      mempool_strdup(s->pool, trim_whitespace(p));
  LOG_INFO("RCPT TO: %s", s->env.recipients[s->env.recipient_count - 1]);

  send_reply(s, 250, "OK");
  // State remains RCPT (can add multiple) or can go to DATA implicitly by
  // commands
}

static void process_command(smtp_session_t *s, char *line) {
  // Parse verb
  char *arg = strchr(line, ' ');
  if (arg) {
    *arg = '\0'; // Split
    arg++;
  } else {
    arg = "";
  }

  if (strcasecmp(line, "EHLO") == 0) {
    process_helo(s, arg, 1);
  } else if (strcasecmp(line, "HELO") == 0) {
    process_helo(s, arg, 0);
  } else if (strcasecmp(line, "MAIL") == 0) {
    // "MAIL FROM:<...>" often comes as two words if space after MAIL
    // But our split made "FROM:<...>" the arg.
    // Standard says "MAIL FROM:<...>"
    if (strncasecmp(arg, "FROM:", 5) == 0) {
      process_mail(s, arg);
    } else {
      send_reply(s, 501, "Syntax error");
    }
  } else if (strcasecmp(line, "RCPT") == 0) {
    if (strncasecmp(arg, "TO:", 3) == 0) {
      process_rcpt(s, arg);
    } else {
      send_reply(s, 501, "Syntax error");
    }
  } else if (strcasecmp(line, "DATA") == 0) {
    if (s->env.recipient_count == 0) {
      send_reply(s, 503, "Need RCPT first");
    } else {
      s->store_ctx = storage_open(NULL); // Auto-generate ID
      if (!s->store_ctx) {
        send_reply(s, 451, "Local error in processing");
        // Stay in command mode? Or close?
        // RFC says 451 means requested action aborted.
        s->state = SMTP_STATE_MAIL; // Reset or keep previous?
        // Simplest to just reset state to wait for MAIL or stay?
        // Actually DATA failure usually resets transaction.
      } else {
        // Persist Envelope Headers for Relay
        if (s->env.sender) {
          char hdr[512];
          snprintf(hdr, sizeof(hdr), "X-Envelope-From: %s\r\n", s->env.sender);
          storage_write(s->store_ctx, hdr, strlen(hdr));
        }
        for (int i = 0; i < s->env.recipient_count; i++) {
          char hdr[512];
          snprintf(hdr, sizeof(hdr), "X-Envelope-To: %s\r\n",
                   s->env.recipients[i]);
          storage_write(s->store_ctx, hdr, strlen(hdr));
        }

        send_reply(s, 354, "Start mail input; end with <CRLF>.<CRLF>");
        s->state = SMTP_STATE_DATA_CONTENT;
      }
    }
  } else if (strcasecmp(line, "RSET") == 0) {
    send_reply(s, 250, "Reset OK");
    s->state = SMTP_STATE_MAIL;
    s->env.recipient_count = 0;
    s->env.sender = NULL;
  } else if (strncasecmp(line, "RSET", 4) == 0) {
    send_reply(s, 250, "OK");
    s->state = SMTP_STATE_INIT; // Reset state
  } else if (strncasecmp(line, "STARTTLS", 8) == 0) {
    process_starttls(s, line + 8);
  } else if (strncasecmp(line, "QUIT", 4) == 0) {
    send_reply(s, 250, "OK");
    s->state = SMTP_STATE_QUIT;
    connection_close(s->conn);
  } else if (strcasecmp(line, "NOOP") == 0) {
    send_reply(s, 250, "OK");
  } else {
    send_reply(s, 500, "Command unrecognized");
  }
}

void smtp_process(smtp_session_t *s) {
  // Read loop
  while (1) {
    char temp[2048];
    // Peek to see if we have valid line
    size_t len = buffer_peek(s->conn->in_buf, temp, sizeof(temp) - 1);
    if (len == 0)
      return;
    temp[len] = 0;

    char *nl = strstr(temp, "\n");
    if (!nl) {
      // No full line yet
      if (len == sizeof(temp) - 1) {
        // Line too long?
        // Consume and clear? Or error?
        buffer_read(s->conn->in_buf, NULL, len);
        send_reply(s, 500, "Line too long");
      }
      return;
    }

    size_t line_len = nl - temp + 1;

    // Read the actual line
    buffer_read(s->conn->in_buf, s->cmd_buffer, line_len);
    s->cmd_buffer[line_len] = 0;

    // Trim \r\n
    while (line_len > 0 && (s->cmd_buffer[line_len - 1] == '\r' ||
                            s->cmd_buffer[line_len - 1] == '\n')) {
      s->cmd_buffer[line_len - 1] = '\0';
      line_len--;
    }

    if (s->state == SMTP_STATE_DATA_CONTENT) {
      if (storage_close(s->store_ctx) == 0) {
        send_reply(s, 250, "OK Message accepted");
      } else {
        send_reply(s, 451, "Failed to commit message");
      }
      s->store_ctx = NULL;
      s->state = SMTP_STATE_MAIL;
      s->env.sender = NULL;
      s->env.recipient_count = 0;
      LOG_INFO("Message transaction completed");
    } else {
      // Dot-stuffing handling: if line starts with "..", skip first dot.
      char *data_to_write = s->cmd_buffer;
      if (s->cmd_buffer[0] == '.' && s->cmd_buffer[1] == '.') {
        data_to_write++;
      }

      // Write line + \n (since we stripped it)
      // Ideally buffer write shouldn't rely on us adding \n back if we want
      // exact preservation But for EML, \n is fine.
      storage_write(s->store_ctx, data_to_write, strlen(data_to_write));
      storage_write(s->store_ctx, "\n", 1);
    }
  }
  else {
    process_command(s, s->cmd_buffer);
  }
}
}
