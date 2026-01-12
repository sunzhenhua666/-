#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *data;
  size_t size;  // Total capacity
  size_t read;  // Read index
  size_t write; // Write index
  size_t count; // Current usage
} buffer_t;

// Create a new ring buffer
buffer_t *buffer_create(size_t size);

// Destroy buffer
void buffer_destroy(buffer_t *buf);

// Write data to buffer
// Returns bytes written
size_t buffer_write(buffer_t *buf, const void *data, size_t len);

// Read data from buffer
// Returns bytes read
size_t buffer_read(buffer_t *buf, void *data, size_t len);

// Peek data without advancing read pointer
size_t buffer_peek(buffer_t *buf, void *data, size_t len);

// Get available space
size_t buffer_available(buffer_t *buf);

// Get used space
size_t buffer_used(buffer_t *buf);

// Reset buffer logic
void buffer_reset(buffer_t *buf);

#endif // BUFFER_H
