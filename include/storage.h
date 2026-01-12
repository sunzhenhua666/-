#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

typedef struct storage_ctx storage_ctx_t;

// Initialize storage subsystem (mkdir, etc)
int storage_init(const char *base_path);

// Open a new storage transaction for a mail
// Returns context handle or NULL on failure
storage_ctx_t *storage_open(const char *queue_id);

// Append data to the open storage file
int storage_write(storage_ctx_t *ctx, const char *data, size_t len);

// Commit and close (move from tmp to new?)
int storage_close(storage_ctx_t *ctx);

// Abort and delete
void storage_abort(storage_ctx_t *ctx);

#endif // STORAGE_H
