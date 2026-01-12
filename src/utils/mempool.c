#include "mempool.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_POOL_CHUNK_SIZE 4096
#define ALIGNMENT 8

typedef struct pool_chunk {
  struct pool_chunk *next;
  char *data;
  size_t size; // Total size of data
  size_t used; // Used bytes
} pool_chunk_t;

struct mempool {
  pool_chunk_t *chunks;
  pool_chunk_t *current;
  size_t default_chunk_size;
};

static pool_chunk_t *create_chunk(size_t size) {
  pool_chunk_t *chunk = malloc(sizeof(pool_chunk_t));
  if (!chunk)
    return NULL;

  chunk->data = malloc(size);
  if (!chunk->data) {
    free(chunk);
    return NULL;
  }

  chunk->size = size;
  chunk->used = 0;
  chunk->next = NULL;
  return chunk;
}

mempool_t *mempool_create(size_t size_hint) {
  mempool_t *pool = malloc(sizeof(mempool_t));
  if (!pool)
    return NULL;

  pool->default_chunk_size =
      size_hint > 0 ? size_hint : DEFAULT_POOL_CHUNK_SIZE;
  pool->chunks = create_chunk(pool->default_chunk_size);
  if (!pool->chunks) {
    free(pool);
    return NULL;
  }
  pool->current = pool->chunks;
  return pool;
}

void mempool_destroy(mempool_t *pool) {
  if (!pool)
    return;

  pool_chunk_t *chk = pool->chunks;
  while (chk) {
    pool_chunk_t *next = chk->next;
    free(chk->data);
    free(chk);
    chk = next;
  }
  free(pool);
}

static void *alloc_from_chunk(pool_chunk_t *chunk, size_t size) {
  // Align size
  size_t aligned_size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

  if (chunk->used + aligned_size <= chunk->size) {
    void *ptr = chunk->data + chunk->used;
    chunk->used += aligned_size;
    return ptr;
  }
  return NULL;
}

void *mempool_alloc(mempool_t *pool, size_t size) {
  if (!pool)
    return NULL;

  // Try current chunk
  void *ptr = alloc_from_chunk(pool->current, size);
  if (ptr)
    return ptr;

  // Allocate new chunk
  // If size is larger than default chunk, likely a large object, give it its
  // own chunk
  size_t new_size =
      size > pool->default_chunk_size ? size : pool->default_chunk_size;
  pool_chunk_t *new_chunk = create_chunk(new_size);
  if (!new_chunk)
    return NULL;

  // Link new chunk
  if (size > pool->default_chunk_size) {
    // For very large allocs, insert as second chunk to keep current "hot" if it
    // has space? Or just append. Simple append for now. Actually, if it's
    // large, we might want to put it elsewhere, but strictly append is O(1) if
    // we tracked tail. We only track head and current. Let's insert after
    // current.
    new_chunk->next = pool->current->next;
    pool->current->next = new_chunk;
    // Don't update current if it's a large blob, unless we want to fill it?
    // If it's exact size, it's full.
  } else {
    // Standard chunk, make it current
    new_chunk->next = pool->chunks; // Push to head for simplicity or...
    // Wait, if we push to head, we lose access to old chunks?
    // No, chunks list head is pool->chunks.
    // Let's prepend to list, but update current.
    new_chunk->next = pool->chunks;
    pool->chunks = new_chunk;
    pool->current = new_chunk;
  }

  // Now alloc from the new chunk (which is either current or the large one)
  // If we made it current:
  if (pool->current == new_chunk) {
    return alloc_from_chunk(new_chunk, size);
  } else {
    // It was a large chunk inserted after current, alloc from it
    // (Wait, logic above was: insert after current. So it is reachable via
    // current->next) But we want to return the ptr. Re-read logic: Large
    // allocs: we want to alloc `size` bytes. We created `new_chunk` of `size`.
    // `alloc_from_chunk` will succeed.
    // The pointer is `new_chunk->data`.
    // We already linked it.
    ptr = alloc_from_chunk(new_chunk, size);
    return ptr;
  }
}

void *mempool_alloc0(mempool_t *pool, size_t size) {
  void *ptr = mempool_alloc(pool, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

char *mempool_strdup(mempool_t *pool, const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s) + 1;
  char *ptr = mempool_alloc(pool, len);
  if (ptr) {
    memcpy(ptr, s, len);
  }
  return ptr;
}

void mempool_free(mempool_t *pool, void *ptr) {
  // No-op for this simple pool (arena/region based)
  (void)pool;
  (void)ptr;
}
