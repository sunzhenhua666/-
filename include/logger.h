#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL
} log_level_t;

typedef enum { LOG_OUTPUT_STDOUT, LOG_OUTPUT_FILE } log_output_t;

// Initialize the logger
int logger_init(log_output_t output, const char *logfile, log_level_t level);

// Destroy the logger
void logger_destroy(void);

// Set minimum log level
void logger_set_level(log_level_t level);

// Internal log function (use macros instead)
void logger_log(log_level_t level, const char *file, int line, const char *fmt,
                ...);

// Macros for easy logging
#define LOG_DEBUG(fmt, ...)                                                    \
  logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                                     \
  logger_log(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                    \
  logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)                                                    \
  logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
