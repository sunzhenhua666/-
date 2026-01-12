#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
  struct {
    int port;
    int ssl_port;
    char *bind_address;
    int max_connections;
    char *cert_file;
    char *key_file;
  } server;

  struct {
    char *path;
    int max_size_mb;
  } storage;

  struct {
    char *level;
    char *file;
  } logging;

  struct {
    char *host;
    int port;
    int relay_threads;
  } upstream;

} config_t;

// Configuration validation result
typedef struct {
  int valid;             // 1 if valid, 0 if invalid
  char error_msg[512];   // Detailed error message
  char error_field[128]; // Field that caused the error
} config_validation_result_t;

// Load configuration from YAML file
config_t *config_load(const char *path);

// Validate configuration
// Returns 0 on success, -1 on validation failure
int config_validate(const config_t *cfg, config_validation_result_t *result);

// Destroy configuration object
void config_destroy(config_t *config);

#endif // CONFIG_H
