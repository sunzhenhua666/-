#include "config_reload.h"
#include "logger.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define MAX_CALLBACKS 16
#define INOTIFY_EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

// Global state
static char *g_config_path = NULL;
static config_t **g_global_config_ptr = NULL;
static pthread_rwlock_t g_config_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static config_reload_callback_t g_callbacks[MAX_CALLBACKS];
static int g_callback_count = 0;
static pthread_t g_monitor_thread;
static volatile int g_running = 0;
static int g_inotify_fd = -1;
static int g_watch_descriptor = -1;

// Forward declaration
static void *inotify_monitor_thread(void *arg);

// ========== Public API ==========

int config_reload_init(const char *config_path, config_t **global_config_ptr) {
  if (!config_path || !global_config_ptr) {
    LOG_ERROR("config_reload_init: invalid arguments");
    return -1;
  }

  g_config_path = strdup(config_path);
  g_global_config_ptr = global_config_ptr;
  g_callback_count = 0;
  g_running = 1;

  // Initialize inotify
  g_inotify_fd = inotify_init();
  if (g_inotify_fd < 0) {
    LOG_ERROR("Failed to initialize inotify: %s", strerror(errno));
    free(g_config_path);
    return -1;
  }

  // Watch for file modifications
  g_watch_descriptor = inotify_add_watch(g_inotify_fd, g_config_path,
                                         IN_MODIFY | IN_CLOSE_WRITE);
  if (g_watch_descriptor < 0) {
    LOG_ERROR("Failed to add inotify watch on %s: %s", g_config_path,
              strerror(errno));
    close(g_inotify_fd);
    free(g_config_path);
    return -1;
  }

  // Start monitoring thread
  if (pthread_create(&g_monitor_thread, NULL, inotify_monitor_thread, NULL) !=
      0) {
    LOG_ERROR("Failed to create inotify monitor thread: %s", strerror(errno));
    inotify_rm_watch(g_inotify_fd, g_watch_descriptor);
    close(g_inotify_fd);
    free(g_config_path);
    return -1;
  }

  LOG_INFO("Configuration hot-reload initialized for: %s", config_path);
  return 0;
}

void config_reload_stop(void) {
  if (!g_running) {
    return;
  }

  g_running = 0;

  // Stop monitoring thread
  if (g_monitor_thread) {
    pthread_cancel(g_monitor_thread);
    pthread_join(g_monitor_thread, NULL);
  }

  // Cleanup inotify
  if (g_watch_descriptor >= 0) {
    inotify_rm_watch(g_inotify_fd, g_watch_descriptor);
    g_watch_descriptor = -1;
  }

  if (g_inotify_fd >= 0) {
    close(g_inotify_fd);
    g_inotify_fd = -1;
  }

  if (g_config_path) {
    free(g_config_path);
    g_config_path = NULL;
  }

  LOG_INFO("Configuration hot-reload stopped");
}

int config_reload_trigger(void) {
  if (!g_config_path || !g_global_config_ptr) {
    LOG_ERROR("config_reload_trigger: not initialized");
    return -1;
  }

  LOG_INFO("Triggering configuration reload...");

  // 1. Load new configuration
  config_t *new_cfg = config_load(g_config_path);
  if (!new_cfg) {
    LOG_ERROR("Hot reload failed: cannot load config file %s", g_config_path);
    return -1;
  }

  // 2. Validate new configuration
  config_validation_result_t result;
  if (config_validate(new_cfg, &result) != 0) {
    LOG_ERROR("Hot reload failed: %s (field: %s)", result.error_msg,
              result.error_field);
    config_destroy(new_cfg);
    return -1;
  }

  // 3. Atomic switch (acquire write lock)
  pthread_rwlock_wrlock(&g_config_rwlock);
  config_t *old_cfg = *g_global_config_ptr;
  *g_global_config_ptr = new_cfg;
  pthread_rwlock_unlock(&g_config_rwlock);

  // 4. Notify callbacks
  for (int i = 0; i < g_callback_count; i++) {
    if (g_callbacks[i]) {
      g_callbacks[i](old_cfg, new_cfg);
    }
  }

  // 5. Destroy old configuration
  config_destroy(old_cfg);

  LOG_INFO("Configuration reloaded successfully");
  return 0;
}

int config_reload_register_callback(config_reload_callback_t callback) {
  if (!callback) {
    return -1;
  }

  if (g_callback_count >= MAX_CALLBACKS) {
    LOG_ERROR("Cannot register callback: maximum %d callbacks reached",
              MAX_CALLBACKS);
    return -1;
  }

  g_callbacks[g_callback_count++] = callback;
  LOG_DEBUG("Registered config reload callback #%d", g_callback_count);
  return 0;
}

void config_read_lock(void) { pthread_rwlock_rdlock(&g_config_rwlock); }

void config_read_unlock(void) { pthread_rwlock_unlock(&g_config_rwlock); }

// ========== Internal Implementation ==========

static void *inotify_monitor_thread(void *arg) {
  (void)arg;
  char buffer[INOTIFY_EVENT_BUF_LEN];

  LOG_DEBUG("inotify monitor thread started");

  while (g_running) {
    // Read inotify events (blocking with timeout via select)
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(g_inotify_fd, &fds);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(g_inotify_fd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      LOG_ERROR("select() failed in inotify monitor: %s", strerror(errno));
      break;
    }

    if (ret == 0) {
      // Timeout, continue loop
      continue;
    }

    // Read events
    ssize_t len = read(g_inotify_fd, buffer, sizeof(buffer));
    if (len < 0) {
      if (errno == EINTR) {
        continue;
      }
      LOG_ERROR("read() failed in inotify monitor: %s", strerror(errno));
      break;
    }

    // Process events
    ssize_t i = 0;
    while (i < len) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];

      if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
        LOG_INFO("Configuration file modified, triggering reload...");
        // Add small delay to avoid partial writes
        sleep(1);
        config_reload_trigger();
      }

      i += sizeof(struct inotify_event) + event->len;
    }
  }

  LOG_DEBUG("inotify monitor thread exiting");
  return NULL;
}
