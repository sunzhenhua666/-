#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <sys/param.h> // For MIN

buffer_t *buffer_create(size_t size) {
  buffer_t *buf = malloc(sizeof(buffer_t));
  if (!buf)
    return NULL;

  buf->data = malloc(size);
  if (!buf->data) {
    free(buf);
    return NULL;
  }

  buf->size = size;
  buf->read = 0;
  buf->write = 0;
  buf->count = 0;

  return buf;
}

void buffer_destroy(buffer_t *buf) {
  if (buf) {
    free(buf->data);
    free(buf);
  }
}

size_t buffer_write(buffer_t *buf, const void *data, size_t len) {
  if (len == 0 || buf->count == buf->size)
    return 0;

  size_t available = buf->size - buf->count;
  size_t to_write = (len < available) ? len : available;

  size_t end_space = buf->size - buf->write;
  const uint8_t *curr = data;

  if (to_write <= end_space) {
    memcpy(buf->data + buf->write, curr, to_write);
    buf->write += to_write;
    if (buf->write == buf->size)
      buf->write = 0;
  } else {
    // Write until end
    memcpy(buf->data + buf->write, curr, end_space);
    // Write wrap around
    memcpy(buf->data, curr + end_space, to_write - end_space);
    buf->write = to_write - end_space;
  }

  buf->count += to_write;
  return to_write;
}

size_t buffer_read(buffer_t *buf, void *data, size_t len) {
  if (len == 0 || buf->count == 0)
    return 0;

  size_t to_read = (len < buf->count) ? len : buf->count;
  size_t end_data = buf->size - buf->read;
  uint8_t *curr = data;

  if (to_read <= end_data) {
    memcpy(curr, buf->data + buf->read, to_read);
    buf->read += to_read;
    if (buf->read == buf->size)
      buf->read = 0;
  } else {
    memcpy(curr, buf->data + buf->read, end_data);
    memcpy(curr + end_data, buf->data, to_read - end_data);
    buf->read = to_read - end_data;
  }

  buf->count -= to_read;
  return to_read;
}

size_t buffer_peek(buffer_t *buf, void *data, size_t len) {
  if (len == 0 || buf->count == 0)
    return 0;

  size_t to_read = (len < buf->count) ? len : buf->count;
  size_t end_data = buf->size - buf->read;
  uint8_t *curr = data;

  if (to_read <= end_data) {
    memcpy(curr, buf->data + buf->read, to_read);
  } else {
    memcpy(curr, buf->data + buf->read, end_data);
    memcpy(curr + end_data, buf->data, to_read - end_data);
  }

  return to_read;
}

size_t buffer_available(buffer_t *buf) { return buf->size - buf->count; }

size_t buffer_used(buffer_t *buf) { return buf->count; }

void buffer_reset(buffer_t *buf) {
  buf->read = 0;
  buf->write = 0;
  buf->count = 0;
}
