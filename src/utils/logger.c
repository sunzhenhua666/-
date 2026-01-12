#include "logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static struct {
  log_output_t output;
  FILE *fp;
  log_level_t level;
  pthread_mutex_t lock;
} g_logger = {.output = LOG_OUTPUT_STDOUT, .fp = NULL, .level = LOG_LEVEL_INFO};

static const char *level_strings[] = {"DEBUG", "INFO ", "WARN ", "ERROR",
                                      "FATAL"};

static const char *level_colors[] = {
    "\x1b[36m", // DEBUG - Cyan
    "\x1b[32m", // INFO  - Green
    "\x1b[33m", // WARN  - Yellow
    "\x1b[31m", // ERROR - Red
    "\x1b[35m"  // FATAL - Magenta
};

static const char *reset_color = "\x1b[0m";

int logger_init(log_output_t output, const char *logfile, log_level_t level) {
  g_logger.output = output;
  g_logger.level = level;

  if (pthread_mutex_init(&g_logger.lock, NULL) != 0) {
    return -1;
  }

  if (output == LOG_OUTPUT_FILE) {
    if (!logfile)
      return -1;
    g_logger.fp = fopen(logfile, "a");
    if (!g_logger.fp) {
      pthread_mutex_destroy(&g_logger.lock);
      return -1;
    }
  } else {
    g_logger.fp = stdout;
  }

  return 0;
}

void logger_destroy(void) {
  pthread_mutex_lock(&g_logger.lock);
  if (g_logger.output == LOG_OUTPUT_FILE && g_logger.fp) {
    fclose(g_logger.fp);
    g_logger.fp = NULL;
  }
  pthread_mutex_unlock(&g_logger.lock);
  pthread_mutex_destroy(&g_logger.lock);
}

void logger_set_level(log_level_t level) { g_logger.level = level; }

void logger_log(log_level_t level, const char *file, int line, const char *fmt,
                ...) {
  if (level < g_logger.level)
    return;

  // Get current time
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);
  char time_buf[20];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_now);

  pthread_mutex_lock(&g_logger.lock);

  FILE *fp = g_logger.fp ? g_logger.fp : stdout;

  if (g_logger.output == LOG_OUTPUT_STDOUT) {
    fprintf(fp, "%s %s%s%s %s:%d: ", time_buf, level_colors[level],
            level_strings[level], reset_color, file, line);
  } else {
    fprintf(fp, "%s %s %s:%d: ", time_buf, level_strings[level], file, line);
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fprintf(fp, "\n");
  fflush(fp);

  pthread_mutex_unlock(&g_logger.lock);
}
