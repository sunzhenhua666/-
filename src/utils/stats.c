#include "stats.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局统计实例
static stats_t g_stats_instance;
stats_t *g_stats = &g_stats_instance;

// ========== 初始化与销毁 ==========

int stats_init(void) {
  memset(g_stats, 0, sizeof(stats_t));
  g_stats->start_time = time(NULL);
  g_stats->last_reset = g_stats->start_time;
  LOG_INFO("Statistics module initialized");
  return 0;
}

void stats_destroy(void) {
  LOG_INFO("Statistics module destroyed");
  // 原子变量无需显式销毁
}

// ========== 原子操作 ==========

void stats_inc(atomic_counter_t *counter) {
  atomic_fetch_add(&counter->value, 1);
}

void stats_dec(atomic_counter_t *counter) {
  atomic_fetch_sub(&counter->value, 1);
}

void stats_add(atomic_counter_t *counter, uint64_t value) {
  atomic_fetch_add(&counter->value, value);
}

uint64_t stats_get(const atomic_counter_t *counter) {
  return atomic_load(&counter->value);
}

// ========== 快照与重置 ==========

void stats_snapshot(stats_t *snapshot) {
  if (!snapshot)
    return;

  // 原子读取所有计数器
  snapshot->active_connections =
      (atomic_counter_t){.value = stats_get(&g_stats->active_connections)};
  snapshot->total_connections =
      (atomic_counter_t){.value = stats_get(&g_stats->total_connections)};
  snapshot->rejected_connections =
      (atomic_counter_t){.value = stats_get(&g_stats->rejected_connections)};

  snapshot->emails_received =
      (atomic_counter_t){.value = stats_get(&g_stats->emails_received)};
  snapshot->emails_stored =
      (atomic_counter_t){.value = stats_get(&g_stats->emails_stored)};
  snapshot->emails_rejected =
      (atomic_counter_t){.value = stats_get(&g_stats->emails_rejected)};

  snapshot->relay_success =
      (atomic_counter_t){.value = stats_get(&g_stats->relay_success)};
  snapshot->relay_failed =
      (atomic_counter_t){.value = stats_get(&g_stats->relay_failed)};
  snapshot->relay_retrying =
      (atomic_counter_t){.value = stats_get(&g_stats->relay_retrying)};
  snapshot->relay_queue_depth =
      (atomic_counter_t){.value = stats_get(&g_stats->relay_queue_depth)};

  snapshot->tls_handshakes =
      (atomic_counter_t){.value = stats_get(&g_stats->tls_handshakes)};
  snapshot->tls_errors =
      (atomic_counter_t){.value = stats_get(&g_stats->tls_errors)};

  snapshot->start_time = g_stats->start_time;
  snapshot->last_reset = g_stats->last_reset;
}

void stats_reset(void) {
  // 保留累计值 (total_connections 等)，重置瞬时值
  atomic_store(&g_stats->active_connections.value, 0);
  atomic_store(&g_stats->relay_queue_depth.value, 0);

  g_stats->last_reset = time(NULL);
  LOG_INFO("Statistics reset (keeping cumulative counters)");
}

// ========== 格式化输出 ==========

void stats_format_text(char *buf, size_t size) {
  if (!buf || size == 0)
    return;

  time_t now = time(NULL);
  time_t uptime = now - g_stats->start_time;

  snprintf(
      buf, size,
      "=== SMTP Relay Statistics ===\n"
      "Uptime: %lds (%ldh %ldm)\n"
      "\n"
      "[Connections]\n"
      "  Active:   %lu\n"
      "  Total:    %lu\n"
      "  Rejected: %lu\n"
      "\n"
      "[Emails]\n"
      "  Received: %lu\n"
      "  Stored:   %lu\n"
      "  Rejected: %lu\n"
      "\n"
      "[Relay]\n"
      "  Success:     %lu\n"
      "  Failed:      %lu\n"
      "  Queue Depth: %lu\n"
      "\n"
      "[TLS]\n"
      "  Handshakes: %lu\n"
      "  Errors:     %lu\n"
      "\n"
      "Last Reset: %lds ago\n",
      uptime, uptime / 3600, (uptime % 3600) / 60,
      stats_get(&g_stats->active_connections),
      stats_get(&g_stats->total_connections),
      stats_get(&g_stats->rejected_connections),
      stats_get(&g_stats->emails_received), stats_get(&g_stats->emails_stored),
      stats_get(&g_stats->emails_rejected), stats_get(&g_stats->relay_success),
      stats_get(&g_stats->relay_failed), stats_get(&g_stats->relay_queue_depth),
      stats_get(&g_stats->tls_handshakes), stats_get(&g_stats->tls_errors),
      now - g_stats->last_reset);
}

void stats_format_json(char *buf, size_t size) {
  if (!buf || size == 0)
    return;

  time_t now = time(NULL);
  time_t uptime = now - g_stats->start_time;

  snprintf(
      buf, size,
      "{\n"
      "  \"uptime_seconds\": %ld,\n"
      "  \"connections\": {\n"
      "    \"active\": %lu,\n"
      "    \"total\": %lu,\n"
      "    \"rejected\": %lu\n"
      "  },\n"
      "  \"emails\": {\n"
      "    \"received\": %lu,\n"
      "    \"stored\": %lu,\n"
      "    \"rejected\": %lu\n"
      "  },\n"
      "  \"relay\": {\n"
      "    \"success\": %lu,\n"
      "    \"failed\": %lu,\n"
      "    \"queue_depth\": %lu\n"
      "  },\n"
      "  \"tls\": {\n"
      "    \"handshakes\": %lu,\n"
      "    \"errors\": %lu\n"
      "  },\n"
      "  \"timestamps\": {\n"
      "    \"start_time\": %ld,\n"
      "    \"last_reset\": %ld,\n"
      "    \"current_time\": %ld\n"
      "  }\n"
      "}",
      uptime, stats_get(&g_stats->active_connections),
      stats_get(&g_stats->total_connections),
      stats_get(&g_stats->rejected_connections),
      stats_get(&g_stats->emails_received), stats_get(&g_stats->emails_stored),
      stats_get(&g_stats->emails_rejected), stats_get(&g_stats->relay_success),
      stats_get(&g_stats->relay_failed), stats_get(&g_stats->relay_queue_depth),
      stats_get(&g_stats->tls_handshakes), stats_get(&g_stats->tls_errors),
      g_stats->start_time, g_stats->last_reset, now);
}
