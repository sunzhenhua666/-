#ifndef CONFIG_RELOAD_H
#define CONFIG_RELOAD_H

#include "config.h"

// Callback function type for configuration reload events
// Called after new configuration has been successfully loaded and validated
// Parameters: old_cfg - previous configuration, new_cfg - new configuration
typedef void (*config_reload_callback_t)(const config_t *old_cfg,
                                         const config_t *new_cfg);

// Initialize configuration hot-reload monitoring
// config_path: path to YAML configuration file
// global_config_ptr: pointer to global config pointer (will be updated on
// reload) Returns 0 on success, -1 on error
int config_reload_init(const char *config_path, config_t **global_config_ptr);

// Stop configuration hot-reload monitoring and cleanup resources
void config_reload_stop(void);

// Manually trigger configuration reload (called by SIGHUP signal handler)
// Returns 0 on success, -1 on error
int config_reload_trigger(void);

// Register a callback to be notified when configuration is reloaded
// callback: function to call on reload
// Returns 0 on success, -1 if too many callbacks registered
int config_reload_register_callback(config_reload_callback_t callback);

// Acquire read lock for safe configuration access
// Call this before reading global configuration in multi-threaded code
void config_read_lock(void);

// Release read lock
void config_read_unlock(void);

#endif // CONFIG_RELOAD_H
