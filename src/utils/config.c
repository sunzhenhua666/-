#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

static void process_server_section(yaml_document_t *doc, yaml_node_t *node,
                                   config_t *cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_item_t *item = node->data.mapping.pairs.start;
       item < node->data.mapping.pairs.top; ++item) {
    yaml_node_t *key = yaml_document_get_node(doc, item->key);
    yaml_node_t *value = yaml_document_get_node(doc, item->value);

    if (!key || !value)
      continue;
    const char *k = (const char *)key->data.scalar.value;

    if (strcmp(k, "port") == 0) {
      cfg->server.port = atoi((const char *)value->data.scalar.value);
    } else if (strcmp(k, "ssl_port") == 0) {
      cfg->server.ssl_port = atoi((const char *)value->data.scalar.value);
    } else if (strcmp(k, "max_connections") == 0) {
      cfg->server.max_connections =
          atoi((const char *)value->data.scalar.value);
    } else if (strcmp(k, "bind_address") == 0) {
      if (cfg->server.bind_address)
        free(cfg->server.bind_address);
      cfg->server.bind_address = strdup((const char *)value->data.scalar.value);
    } else if (strcmp(k, "cert_file") == 0) {
      if (cfg->server.cert_file)
        free(cfg->server.cert_file);
      cfg->server.cert_file = strdup((const char *)value->data.scalar.value);
    } else if (strcmp(k, "key_file") == 0) {
      if (cfg->server.key_file)
        free(cfg->server.key_file);
      cfg->server.key_file = strdup((const char *)value->data.scalar.value);
    }
  }
}

static void process_logging_section(yaml_document_t *doc, yaml_node_t *node,
                                    config_t *cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_item_t *item = node->data.mapping.pairs.start;
       item < node->data.mapping.pairs.top; ++item) {
    yaml_node_t *key = yaml_document_get_node(doc, item->key);
    yaml_node_t *value = yaml_document_get_node(doc, item->value);

    if (!key || !value)
      continue;
    const char *k = (const char *)key->data.scalar.value;

    if (strcmp(k, "level") == 0) {
      if (cfg->logging.level)
        free(cfg->logging.level);
      cfg->logging.level = strdup((const char *)value->data.scalar.value);
    } else if (strcmp(k, "file") == 0) {
      if (cfg->logging.file)
        free(cfg->logging.file);
      cfg->logging.file = strdup((const char *)value->data.scalar.value);
    }
  }
}

static void process_storage_section(yaml_document_t *doc, yaml_node_t *node,
                                    config_t *cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_item_t *item = node->data.mapping.pairs.start;
       item < node->data.mapping.pairs.top; ++item) {
    yaml_node_t *key = yaml_document_get_node(doc, item->key);
    yaml_node_t *value = yaml_document_get_node(doc, item->value);

    if (!key || !value)
      continue;
    const char *k = (const char *)key->data.scalar.value;

    if (strcmp(k, "path") == 0) {
      if (cfg->storage.path)
        free(cfg->storage.path);
      cfg->storage.path = strdup((const char *)value->data.scalar.value);
    } else if (strcmp(k, "max_size_mb") == 0) {
      cfg->storage.max_size_mb = atoi((const char *)value->data.scalar.value);
    }
  }
}

static void process_upstream_section(yaml_document_t *doc, yaml_node_t *node,
                                     config_t *cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_item_t *item = node->data.mapping.pairs.start;
       item < node->data.mapping.pairs.top; ++item) {
    yaml_node_t *key = yaml_document_get_node(doc, item->key);
    yaml_node_t *value = yaml_document_get_node(doc, item->value);

    if (!key || !value)
      continue;
    const char *k = (const char *)key->data.scalar.value;

    if (strcmp(k, "host") == 0) {
      if (cfg->upstream.host)
        free(cfg->upstream.host);
      cfg->upstream.host = strdup((const char *)value->data.scalar.value);
    } else if (strcmp(k, "port") == 0) {
      cfg->upstream.port = atoi((const char *)value->data.scalar.value);
    } else if (strcmp(k, "relay_threads") == 0) {
      cfg->upstream.relay_threads =
          atoi((const char *)value->data.scalar.value);
    }
  }
}

config_t *config_load(const char *path) {
  FILE *fh = fopen(path, "r");
  if (!fh) {
    LOG_FATAL("Failed to open config file: %s", path);
    return NULL;
  }

  yaml_parser_t parser;
  if (!yaml_parser_initialize(&parser)) {
    LOG_FATAL("Failed to initialize yaml parser");
    fclose(fh);
    return NULL;
  }

  yaml_parser_set_input_file(&parser, fh);
  yaml_document_t doc;

  if (!yaml_parser_load(&parser, &doc)) {
    LOG_FATAL("Failed to parse YAML document");
    yaml_parser_delete(&parser);
    fclose(fh);
    return NULL;
  }

  config_t *cfg = calloc(1, sizeof(config_t));
  // Set Defaults
  cfg->server.port = 25;
  cfg->server.ssl_port = 465;
  cfg->server.max_connections = 1000;
  cfg->server.bind_address = strdup("0.0.0.0");
  cfg->storage.max_size_mb = 10240;
  cfg->logging.level = strdup("INFO");

  yaml_node_t *root = yaml_document_get_root_node(&doc);
  if (root && root->type == YAML_MAPPING_NODE) {
    for (yaml_node_item_t *item = root->data.mapping.pairs.start;
         item < root->data.mapping.pairs.top; ++item) {
      yaml_node_t *key = yaml_document_get_node(&doc, item->key);
      yaml_node_t *value = yaml_document_get_node(&doc, item->value);

      if (!key || !value)
        continue;
      const char *k = (const char *)key->data.scalar.value;

      if (strcmp(k, "server") == 0) {
        process_server_section(&doc, value, cfg);
      } else if (strcmp(k, "logging") == 0) {
        process_logging_section(&doc, value, cfg);
      } else if (strcmp(k, "storage") == 0) {
        process_storage_section(&doc, value, cfg);
      } else if (strcmp(k, "upstream") == 0) {
        process_upstream_section(&doc, value, cfg);
      }
    }
  }

  yaml_document_delete(&doc);
  yaml_parser_delete(&parser);
  fclose(fh);

  LOG_INFO("Config loaded: bind=%s port=%d", cfg->server.bind_address,
           cfg->server.port);
  return cfg;
}

void config_destroy(config_t *config) {
  if (!config)
    return;
  if (config->server.bind_address)
    free(config->server.bind_address);
  if (config->server.cert_file)
    free(config->server.cert_file);
  if (config->server.key_file)
    free(config->server.key_file);
  if (config->storage.path)
    free(config->storage.path);
  if (config->logging.level)
    free(config->logging.level);
  if (config->logging.file)
    free(config->logging.file);
  if (config->upstream.host)
    free(config->upstream.host);
  free(config);
}

// ========== Configuration Validation ==========

#include <sys/stat.h>
#include <unistd.h>

// Validate port range (1-65535)
static int validate_port(int port, const char *field_name,
                         config_validation_result_t *result) {
  if (port < 1 || port > 65535) {
    snprintf(result->error_field, sizeof(result->error_field), "%s",
             field_name);
    snprintf(result->error_msg, sizeof(result->error_msg),
             "%s must be between 1 and 65535 (got %d)", field_name, port);
    return 0;
  }
  return 1;
}

// Validate path exists and is a directory
static int validate_path(const char *path, const char *field_name,
                         config_validation_result_t *result) {
  if (!path || strlen(path) == 0) {
    snprintf(result->error_field, sizeof(result->error_field), "%s",
             field_name);
    snprintf(result->error_msg, sizeof(result->error_msg), "%s cannot be empty",
             field_name);
    return 0;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    snprintf(result->error_field, sizeof(result->error_field), "%s",
             field_name);
    snprintf(result->error_msg, sizeof(result->error_msg),
             "%s path does not exist: %s", field_name, path);
    return 0;
  }

  if (!S_ISDIR(st.st_mode)) {
    snprintf(result->error_field, sizeof(result->error_field), "%s",
             field_name);
    snprintf(result->error_msg, sizeof(result->error_msg),
             "%s must be a directory: %s", field_name, path);
    return 0;
  }

  return 1;
}

// Validate log level
static int validate_log_level(const char *level, const char *field_name,
                              config_validation_result_t *result) {
  if (!level || strlen(level) == 0) {
    return 1; // Optional field
  }

  const char *valid_levels[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
  for (size_t i = 0; i < sizeof(valid_levels) / sizeof(valid_levels[0]); i++) {
    if (strcasecmp(level, valid_levels[i]) == 0) {
      return 1;
    }
  }

  snprintf(result->error_field, sizeof(result->error_field), "%s", field_name);
  snprintf(result->error_msg, sizeof(result->error_msg),
           "%s must be one of: DEBUG, INFO, WARN, ERROR, FATAL (got %s)",
           field_name, level);
  return 0;
}

// Main validation function
int config_validate(const config_t *cfg, config_validation_result_t *result) {
  if (!cfg || !result) {
    return -1;
  }

  memset(result, 0, sizeof(config_validation_result_t));
  result->valid = 0;

  // Validate server.port
  if (!validate_port(cfg->server.port, "server.port", result)) {
    return -1;
  }

  // Validate server.ssl_port
  if (!validate_port(cfg->server.ssl_port, "server.ssl_port", result)) {
    return -1;
  }

  // Ensure ports don't conflict
  if (cfg->server.port == cfg->server.ssl_port) {
    snprintf(result->error_field, sizeof(result->error_field), "server.port");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "server.port (%d) and server.ssl_port (%d) cannot be the same",
             cfg->server.port, cfg->server.ssl_port);
    return -1;
  }

  // Validate max_connections
  if (cfg->server.max_connections < 1 || cfg->server.max_connections > 100000) {
    snprintf(result->error_field, sizeof(result->error_field),
             "server.max_connections");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "server.max_connections must be between 1 and 100000 (got %d)",
             cfg->server.max_connections);
    return -1;
  }

  // Validate bind_address
  if (!cfg->server.bind_address || strlen(cfg->server.bind_address) == 0) {
    snprintf(result->error_field, sizeof(result->error_field),
             "server.bind_address");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "server.bind_address cannot be empty");
    return -1;
  }

  // Validate storage.path
  if (!validate_path(cfg->storage.path, "storage.path", result)) {
    return -1;
  }

  // Validate storage.max_size_mb
  if (cfg->storage.max_size_mb < 1) {
    snprintf(result->error_field, sizeof(result->error_field),
             "storage.max_size_mb");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "storage.max_size_mb must be greater than 0 (got %d)",
             cfg->storage.max_size_mb);
    return -1;
  }

  // Validate logging.level
  if (!validate_log_level(cfg->logging.level, "logging.level", result)) {
    return -1;
  }

  // Validate upstream.host
  if (!cfg->upstream.host || strlen(cfg->upstream.host) == 0) {
    snprintf(result->error_field, sizeof(result->error_field), "upstream.host");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "upstream.host cannot be empty");
    return -1;
  }

  // Validate upstream.port
  if (!validate_port(cfg->upstream.port, "upstream.port", result)) {
    return -1;
  }

  // Validate upstream.relay_threads
  if (cfg->upstream.relay_threads < 1 || cfg->upstream.relay_threads > 64) {
    snprintf(result->error_field, sizeof(result->error_field),
             "upstream.relay_threads");
    snprintf(result->error_msg, sizeof(result->error_msg),
             "upstream.relay_threads must be between 1 and 64 (got %d)",
             cfg->upstream.relay_threads);
    return -1;
  }

  result->valid = 1;
  return 0;
}
