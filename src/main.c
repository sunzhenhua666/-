#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "config_reload.h"
#include "logger.h"
#include "policy.h"
#include "relay.h"
#include "smtp_server.h"
#include "stats.h"
#include "storage.h"
#include "tls.h"

// Global stop flag
volatile sig_atomic_t g_stop = 0;

void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    LOG_INFO("Received signal %d, shutting down...", sig);
    g_stop = 1;
  } else if (sig == SIGHUP) {
    LOG_INFO("Received SIGHUP, triggering configuration reload...");
    config_reload_trigger();
  }
}

// Configuration reload callback
void on_config_reloaded(const config_t *old_cfg, const config_t *new_cfg) {
  LOG_INFO("Configuration reloaded:");
  if (old_cfg->server.port != new_cfg->server.port) {
    LOG_INFO("  server.port: %d -> %d (requires restart)", old_cfg->server.port,
             new_cfg->server.port);
  }
  if (old_cfg->upstream.relay_threads != new_cfg->upstream.relay_threads) {
    LOG_INFO("  upstream.relay_threads: %d -> %d",
             old_cfg->upstream.relay_threads, new_cfg->upstream.relay_threads);
  }
}

void print_usage(const char *prog_name) {
  fprintf(stderr, "Usage: %s [-c config_file] [-d]\n", prog_name);
  fprintf(stderr, "  -c config_file  Path to configuration file (default: "
                  "config/relay.yaml)\n");
  fprintf(stderr, "  -d              Daemon mode (background)\n");
}

int main(int argc, char *argv[]) {
  int opt;
  const char *config_file = "config/relay.yaml";
  int daemon_mode = 0;

  while ((opt = getopt(argc, argv, "c:dh")) != -1) {
    switch (opt) {
    case 'c':
      config_file = optarg;
      break;
    case 'd':
      daemon_mode = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);
    default:
      print_usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Initialize Logger
  if (logger_init(daemon_mode ? LOG_OUTPUT_FILE : LOG_OUTPUT_STDOUT,
                  daemon_mode ? "/var/log/relaymail/relay.log" : NULL,
                  LOG_LEVEL_INFO) != 0) {
    fprintf(stderr, "Failed to initialize logger\n");
    return EXIT_FAILURE;
  }

  LOG_INFO("Starting High-Performance SMTP Relay Gateway...");

  // Setup Signal Handlers
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL); // For config reload

  // Load Configuration
  config_t *config = config_load(config_file);
  if (!config) {
    LOG_FATAL("Failed to load configuration from %s", config_file);
    logger_destroy();
    return EXIT_FAILURE;
  }

  // Validate Configuration
  config_validation_result_t validation_result;
  if (config_validate(config, &validation_result) != 0) {
    LOG_FATAL("Configuration validation failed: %s (field: %s)",
              validation_result.error_msg, validation_result.error_field);
    config_destroy(config);
    logger_destroy();
    return EXIT_FAILURE;
  }
  LOG_INFO("Configuration validated successfully");

  // Initialize Configuration Hot-reload
  if (config_reload_init(config_file, &config) != 0) {
    LOG_WARN("Failed to initialize config hot-reload, running without it");
  } else {
    config_reload_register_callback(on_config_reloaded);
    LOG_INFO("Configuration hot-reload enabled");
  }

  // Initialize Statistics Module
  if (stats_init() != 0) {
    LOG_WARN("Failed to initialize statistics module");
  }

  // Daemonize if requested
  if (daemon_mode) {
    if (daemon(0, 0) == -1) {
      LOG_FATAL("Failed to daemonize process");
      config_destroy(config);
      logger_destroy();
      return EXIT_FAILURE;
    }
  }

  // Initialize Storage
  if (storage_init(config->storage.path) != 0) {
    LOG_FATAL("Failed to initialize storage at %s", config->storage.path);
    config_destroy(config);
    logger_destroy();
    return EXIT_FAILURE;
  }

  // Initialize Policy
  policy_init(config);

  // Initialize TLS
  tls_init_library();
  SSL_CTX *ssl_ctx =
      tls_create_context(config->server.cert_file, config->server.key_file);
  if (ssl_ctx) {
    smtp_server_set_ssl_ctx(ssl_ctx);
  } else {
    LOG_WARN("TLS context could not be created, STARTTLS disabled");
  }

  // Initialize Relay
  if (relay_init(config) != 0) {
    LOG_FATAL("Failed to initialize relay");
    config_destroy(config);
    logger_destroy();
    return EXIT_FAILURE;
  }
  relay_start();

  // TODO: Initialize Reactor
  // TODO: Initialize Thread Pool
  // TODO: Start Server

  LOG_INFO("Server entering main loop");

  while (!g_stop) {
    // Main loop placeholder (e.g., stats reporting, watchdog)
    sleep(1);
  }

  LOG_INFO("Shutting down...");

  relay_stop();
  config_reload_stop();
  stats_destroy();

  // Cleanup
  config_destroy(config);
  logger_destroy();

  return EXIT_SUCCESS;
}
