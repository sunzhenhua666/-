#include "storage.h"
#include "config.h"
#include "logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


struct storage_ctx {
  char *path;
  FILE *fh;
  char *final_path;
};

static char *base_spool_path = NULL;

static int mkdir_p(const char *path) {
  char tmp[1024];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return -1;
  return 0;
}

int storage_init(const char *base_path) {
  if (base_spool_path)
    free(base_spool_path);
  base_spool_path = strdup(base_path);

  char path[1024];
  // Create tmp and new dirs
  snprintf(path, sizeof(path), "%s/tmp", base_path);
  if (mkdir_p(path) != 0)
    return -1;

  snprintf(path, sizeof(path), "%s/new", base_path);
  if (mkdir_p(path) != 0)
    return -1;

  return 0;
}

storage_ctx_t *storage_open(const char *queue_id) {
  if (!base_spool_path)
    return NULL;

  storage_ctx_t *ctx = calloc(1, sizeof(storage_ctx_t));
  if (!ctx)
    return NULL;

  // Generate filename if queue_id is NULL
  char id_buf[64];
  if (!queue_id) {
    snprintf(id_buf, sizeof(id_buf), "%ld.%d", (long)time(NULL), rand());
    queue_id = id_buf;
  }

  // Path: base/tmp/ID.eml
  size_t path_len = strlen(base_spool_path) + strlen(queue_id) + 20;
  ctx->path = malloc(path_len);
  ctx->final_path = malloc(path_len);

  snprintf(ctx->path, path_len, "%s/tmp/%s.eml", base_spool_path, queue_id);
  snprintf(ctx->final_path, path_len, "%s/new/%s.eml", base_spool_path,
           queue_id);

  ctx->fh = fopen(ctx->path, "wb");
  if (!ctx->fh) {
    LOG_ERROR("Failed to open storage file %s: %s", ctx->path, strerror(errno));
    free(ctx->path);
    free(ctx->final_path);
    free(ctx);
    return NULL;
  }

  return ctx;
}

int storage_write(storage_ctx_t *ctx, const char *data, size_t len) {
  if (!ctx || !ctx->fh)
    return -1;
  if (fwrite(data, 1, len, ctx->fh) != len) {
    return -1;
  }
  return 0;
}

int storage_close(storage_ctx_t *ctx) {
  if (!ctx)
    return -1;

  int ret = 0;
  if (ctx->fh) {
    fclose(ctx->fh);
    ctx->fh = NULL;
  }

  // Move from tmp to new
  if (rename(ctx->path, ctx->final_path) != 0) {
    LOG_ERROR("Failed to commit mail %s -> %s: %s", ctx->path, ctx->final_path,
              strerror(errno));
    ret = -1;
    // Try unlink?
    unlink(ctx->path);
  } else {
    LOG_INFO("Mail committed: %s", ctx->final_path);
  }

  if (ctx->path)
    free(ctx->path);
  if (ctx->final_path)
    free(ctx->final_path);
  free(ctx);
  return ret;
}

void storage_abort(storage_ctx_t *ctx) {
  if (!ctx)
    return;
  if (ctx->fh)
    fclose(ctx->fh);
  if (ctx->path) {
    unlink(ctx->path);
    free(ctx->path);
  }
  if (ctx->final_path)
    free(ctx->final_path);
  free(ctx);
}
