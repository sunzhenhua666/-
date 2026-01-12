#ifndef STATS_H
#define STATS_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// 原子计数器类型 (Thread-safe)
typedef struct {
  _Atomic uint64_t value;
} atomic_counter_t;

// 全局统计数据结构
typedef struct {
  // === 连接统计 ===
  atomic_counter_t active_connections;   // 当前活跃连接数
  atomic_counter_t total_connections;    // 累计连接总数
  atomic_counter_t rejected_connections; // 拒绝连接数 (策略/限制)

  // === 邮件处理统计 ===
  atomic_counter_t emails_received; // 接收的邮件总数
  atomic_counter_t emails_stored;   // 成功存储的邮件数
  atomic_counter_t emails_rejected; // 拒绝的邮件数 (策略)

  // === 中继统计 ===
  atomic_counter_t relay_success;     // 中继成功数
  atomic_counter_t relay_failed;      // 中继失败数
  atomic_counter_t relay_retrying;    // 正在重试的邮件数
  atomic_counter_t relay_queue_depth; // 当前队列深度

  // === TLS统计 ===
  atomic_counter_t tls_handshakes; // TLS 握手成功次数
  atomic_counter_t tls_errors;     // TLS 错误次数

  // === 时间戳 ===
  time_t start_time; // 进程启动时间
  time_t last_reset; // 上次重置时间
} stats_t;

// ========== 核心接口 ==========

// 初始化统计系统
int stats_init(void);

// 销毁统计系统
void stats_destroy(void);

// 原子操作: 增加计数器
void stats_inc(atomic_counter_t *counter);

// 原子操作: 减少计数器
void stats_dec(atomic_counter_t *counter);

// 原子操作: 增加指定值
void stats_add(atomic_counter_t *counter, uint64_t value);

// 获取计数器当前值 (原子读取)
uint64_t stats_get(const atomic_counter_t *counter);

// 获取全局统计快照 (线程安全)
void stats_snapshot(stats_t *snapshot);

// 重置统计数据 (保留 start_time)
void stats_reset(void);

// 格式化输出 (文本格式, 用于 CLI)
// buf: 输出缓冲区, size: 缓冲区大小
void stats_format_text(char *buf, size_t size);

// 格式化输出 (JSON格式, 用于 API)
void stats_format_json(char *buf, size_t size);

// ========== 便捷宏 ==========
// 直接访问全局统计实例
extern stats_t *g_stats;

#define STATS_INC_CONNECTIONS() stats_inc(&g_stats->total_connections)
#define STATS_INC_ACTIVE_CONN() stats_inc(&g_stats->active_connections)
#define STATS_DEC_ACTIVE_CONN() stats_dec(&g_stats->active_connections)
#define STATS_INC_REJECTED_CONN() stats_inc(&g_stats->rejected_connections)

#define STATS_INC_EMAILS_RECEIVED() stats_inc(&g_stats->emails_received)
#define STATS_INC_EMAILS_STORED() stats_inc(&g_stats->emails_stored)
#define STATS_INC_EMAILS_REJECTED() stats_inc(&g_stats->emails_rejected)

#define STATS_INC_RELAY_SUCCESS() stats_inc(&g_stats->relay_success)
#define STATS_INC_RELAY_FAILED() stats_inc(&g_stats->relay_failed)
#define STATS_INC_QUEUE_DEPTH() stats_inc(&g_stats->relay_queue_depth)
#define STATS_DEC_QUEUE_DEPTH() stats_dec(&g_stats->relay_queue_depth)

#define STATS_INC_TLS_HANDSHAKES() stats_inc(&g_stats->tls_handshakes)
#define STATS_INC_TLS_ERRORS() stats_inc(&g_stats->tls_errors)

#endif // STATS_H
