#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>

typedef struct mempool mempool_t;

// Create a new memory pool
// size_hint: Initial size of the pool (0 for default)
mempool_t *mempool_create(size_t size_hint);

// Destroy the memory pool and free all allocated memory
void mempool_destroy(mempool_t *pool);

// Allocate memory from the pool
void *mempool_alloc(mempool_t *pool, size_t size);

// Allocate zero-initialized memory
void *mempool_alloc0(mempool_t *pool, size_t size);

// Strdup using pool
char *mempool_strdup(mempool_t *pool, const char *s);

// Free is generally a no-op in simple pointer-bumping pools,
// or allows reuse if using slab list.
// For this v1 implementation, we assume pool lifecycle matches request
// lifecycle, so individual free is not strictly required but provided for
// interface compatibility.
void mempool_free(mempool_t *pool, void *ptr);

#endif // MEMPOOL_H
