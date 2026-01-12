#ifndef POLICY_H
#define POLICY_H

#include "config.h"

// Initialize Policy Engine
int policy_init(config_t *config);

// Check if connection IP is allowed (returns 0 for allowed, non-zero for deny)
int policy_check_connection(const char *ip);

// Check if sender is allowed
int policy_check_sender(const char *sender);

// Check if recipient is allowed
int policy_check_recipient(const char *recipient);

#endif // POLICY_H
