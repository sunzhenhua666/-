#ifndef RELAY_H
#define RELAY_H

#include "config.h"

// Initialize Relay Subsystem
int relay_init(config_t *config);

// Start Relay Worker (blocking or threaded)
void relay_start(void);

// Stop Relay Worker
void relay_stop(void);

#endif // RELAY_H
