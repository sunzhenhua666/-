#include "policy.h"
#include "config.h"
#include "logger.h"
#include <string.h>
#include <strings.h> // for strcasecmp


static config_t *g_config = NULL;

int policy_init(config_t *config) {
  g_config = config;
  LOG_INFO("Policy engine initialized");
  return 0;
}

int policy_check_connection(const char *ip) {
  if (!ip)
    return -1;
  // TODO: Implement IP blacklist check
  LOG_DEBUG("Policy: Connection from %s allowed", ip);
  return 0;
}

int policy_check_sender(const char *sender) {
  if (!sender)
    return -1;
  // TODO: Implement Sender blacklist/whitelist
  // Example: Block spammer@bad.com
  if (strcasecmp(sender, "spammer@bad.com") == 0) {
    LOG_WARN("Policy: Blocked sender %s", sender);
    return 1;
  }
  return 0;
}

int policy_check_recipient(const char *recipient) {
  if (!recipient)
    return -1;
  // TODO: Implement Relay Domain check?
  // For open relay prevention, we should strictly check domains if we are an
  // inbound gateway.
  return 0;
}
