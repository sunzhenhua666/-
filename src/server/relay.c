#include "relay.h"
#include "config.h"
#include "logger.h"
#include "queue.h"
#include "socket_utils.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>


static config_t *g_config = NULL;
static volatile int g_running = 0;
static pthread_t g_relay_thread;
static queue_t *g_work_queue = NULL;
static pthread_t *g_worker_threads = NULL;
static int g_num_workers = 0;
static pthread_t g_scanner_thread;

// Helper: Connect to upstream
static int connect_upstream() {
  if (!g_config || !g_config->upstream.host)
    return -1;

  struct hostent *he = gethostbyname(g_config->upstream.host);
  if (!he) {
    LOG_ERROR("Relay: Failed to resolve host %s", g_config->upstream.host);
    return -1;
  }

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1)
    return -1;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(g_config->upstream.port);
  memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    LOG_ERROR("Relay: Connect failed to %s:%d: %s", g_config->upstream.host,
              g_config->upstream.port, strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}

// Helper: Read line from socket
static int read_reply(int fd, char *buf, size_t size) {
  // Simple blocking read for prototype
  ssize_t n = read(fd, buf, size - 1);
  if (n > 0) {
    buf[n] = 0;
    return atoi(buf);
  }
  return 0;
}

// Helper: Send command
static void send_cmd(int fd, const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  write(fd, buf, strlen(buf));
  // Log debug?
}

// Helper: Extract envelope from file (old, now integrated into
// relay_process_file) static int parse_envelope(FILE *fp, char *sender, char
// **recipients,
//                           int *rcpt_count) {
//   char line[1024];
//   long pos = ftell(fp);
//   fseek(fp, 0, SEEK_SET);

//   *rcpt_count = 0;

//   while (fgets(line, sizeof(line), fp)) {
//     if (strncasecmp(line, "X-Envelope-From:", 16) == 0) {
//       char *p = line + 16;
//       while (*p == ' ')
//         p++;
//       char *nl = strchr(p, '\r');
//       if (nl)
//         *nl = 0;
//       nl = strchr(p, '\n');
//       if (nl)
//         *nl = 0;
//       strcpy(sender, p);
//     } else if (strncasecmp(line, "X-Envelope-To:", 14) == 0) {
//       if (*rcpt_count < 10) {
//         char *p = line + 14;
//         while (*p == ' ')
//           p++;
//         char *nl = strchr(p, '\r');
//         if (nl)
//           *nl = 0;
//         nl = strchr(p, '\n');
//         if (nl)
//           *nl = 0;
//         recipients[(*rcpt_count)++] = strdup(p);
//       }
//     } else {
//       // Assume headers end when logic breaks or empty line?
//       // Actually our smtp.c writes them at very top properly.
//       // If line doesn't start with X-Envelope, maybe we are done?
//       // But strict check is better.
//       if (line[0] != 'X' && line[0] != 'x')
//         break;
//     }
//   }

//   fseek(fp, 0, SEEK_SET); // Reset
//   return 0;
// }

// New relay_process_file function
static int relay_process_file(const char *filepath) {
  LOG_INFO("Relay: Processing %s", filepath);

  FILE *fp = fopen(filepath, "rb");
  if (!fp) {
    LOG_ERROR("Relay: Failed to open file %s: %s", filepath, strerror(errno));
    return -1;
  }

  // 1. Parse Envelope (X-Envelope-From/To)
  char sender[256] = {0};
  char recipients[10][256]; // Max 10 recipients
  int rcpt_count = 0;

  char line[1024];

  // Read headers to extract X-Envelope-From and X-Envelope-To
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '\r' || line[0] == '\n') { // End of headers
      break;
    }
    if (strncasecmp(line, "X-Envelope-From:", 16) == 0) {
      char *p = line + 16;
      while (*p && (*p == ' ' || *p == '<'))
        p++;
      char *end = strchr(p, '>');
      if (end)
        *end = 0;
      else {
        end = strpbrk(p, "\r\n");
        if (end)
          *end = 0;
      }
      strncpy(sender, p, sizeof(sender) - 1);
      sender[sizeof(sender) - 1] = '\0';
    } else if (strncasecmp(line, "X-Envelope-To:", 14) == 0) {
      if (rcpt_count < 10) {
        char *p = line + 14;
        while (*p && (*p == ' ' || *p == '<'))
          p++;
        char *end = strchr(p, '>');
        if (end)
          *end = 0;
        else {
          end = strpbrk(p, "\r\n");
          if (end)
            *end = 0;
        }
        strncpy(recipients[rcpt_count], p, sizeof(recipients[0]) - 1);
        recipients[rcpt_count][sizeof(recipients[0]) - 1] = '\0';
        rcpt_count++;
      }
    }
  }
  rewind(fp); // Reset file pointer to the beginning for streaming

  if (sender[0] == 0 || rcpt_count == 0) {
    LOG_WARN("Relay: No sender or recipients found in %s", filepath);
    fclose(fp);
    return -1;
  }

  // 2. Connect Upstream
  int fd = connect_upstream();
  if (fd == -1) {
    fclose(fp);
    return -1; // Retry later
  }

  // Helper macros for sending/receiving
  char buf[4096];
#define RECV()                                                                 \
  {                                                                            \
    ssize_t n = read(fd, buf, sizeof(buf) - 1);                                \
    if (n < 0) {                                                               \
      LOG_ERROR("Relay: Read error: %s", strerror(errno));                     \
      goto err;                                                                \
    }                                                                          \
    buf[n] = 0;                                                                \
  }
#define SEND(str)                                                              \
  {                                                                            \
    if (write(fd, str, strlen(str)) < 0) {                                     \
      LOG_ERROR("Relay: Write error: %s", strerror(errno));                    \
      goto err;                                                                \
    }                                                                          \
  }
#define EXPECT(code)                                                           \
  {                                                                            \
    RECV();                                                                    \
    if (strncmp(buf, #code, 3) != 0) {                                         \
      LOG_ERROR("Relay: Expected %s, got %s", #code, buf);                     \
      goto err;                                                                \
    }                                                                          \
  }

  EXPECT(220); // Banner

  SEND("EHLO relay.local\r\n");
  EXPECT(250);

  // MAIL FROM
  snprintf(buf, sizeof(buf), "MAIL FROM: <%s>\r\n", sender);
  SEND(buf);
  EXPECT(250);

  // RCPT TO
  for (int i = 0; i < rcpt_count; i++) {
    snprintf(buf, sizeof(buf), "RCPT TO: <%s>\r\n", recipients[i]);
    SEND(buf);
    EXPECT(250);
  }

  SEND("DATA\r\n");
  EXPECT(354);

  // Stream File
  // The file content includes the X-Envelope headers.
  // These will be treated as regular headers by the upstream server.
  while (fgets(line, sizeof(line), fp)) {
    // transparency stuffing: if line starts with .
    if (line[0] == '.')
      write(fd, ".", 1);
    write(fd, line, strlen(line));
  }

  SEND("\r\n.\r\n"); // End of data
  EXPECT(250);

  SEND("QUIT\r\n");
  // EXPECT(221); // QUIT response is often 221, but not strictly necessary to
  // check for success.

  close(fd);
  fclose(fp);
  LOG_INFO("Relay: Successfully delivered %s", filepath);
  return 0;

err:
  LOG_ERROR("Relay: Failed to deliver %s", filepath);
  close(fd);
  fclose(fp);
  return -1;
}

// Scanner thread function
static void *relay_scanner_thread(void *arg) {
  (void)arg;
  char new_path[1024];
  snprintf(new_path, sizeof(new_path), "%s/new", g_config->storage.path);

  char queue_path[1024];
  snprintf(queue_path, sizeof(queue_path), "%s/queue", g_config->storage.path);

  // Ensure queue directory exists
  mkdir(queue_path, 0777);

  LOG_INFO("Relay scanner started on %s, moving files to %s", new_path,
           queue_path);

  while (g_running) {
    DIR *dir = opendir(new_path);
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        if (!g_running)
          break;
        if (entry->d_type == DT_REG) { // Regular file
          char file_path[1024];
          snprintf(file_path, sizeof(file_path), "%s/%s", new_path,
                   entry->d_name);

          char new_file_in_queue_path[1024];
          snprintf(new_file_in_queue_path, sizeof(new_file_in_queue_path),
                   "%s/%s", queue_path, entry->d_name);

          if (rename(file_path, new_file_in_queue_path) == 0) {
            char *q_info = strdup(new_file_in_queue_path);
            if (q_info) {
              queue_push(g_work_queue, q_info);
              LOG_DEBUG("Relay scanner: Queued %s", new_file_in_queue_path);
            } else {
              LOG_ERROR(
                  "Relay scanner: Failed to allocate memory for queue item.");
              // Attempt to move back or log for manual intervention
              rename(new_file_in_queue_path, file_path); // Move back to new
            }
          } else {
            LOG_ERROR("Relay scanner: Failed to move file %s to %s: %s",
                      file_path, new_file_in_queue_path, strerror(errno));
          }
        }
      }
      closedir(dir);
    } else {
      LOG_ERROR("Relay scanner: Failed to open directory %s: %s", new_path,
                strerror(errno));
    }
    sleep(1); // Poll interval
  }
  LOG_INFO("Relay scanner stopped.");
  return NULL;
}

// Worker thread function
static void *relay_worker_thread(void *arg) {
  (void)arg;
  LOG_INFO("Relay worker thread started.");
  while (g_running ||
         !queue_is_empty(g_work_queue)) { // Keep running as long as there's
                                          // work or g_running is true
    char *filepath = (char *)queue_pop(g_work_queue); // Blocking pop

    if (filepath) {
      LOG_DEBUG("Relay worker: Processing %s", filepath);
      if (relay_process_file(filepath) == 0) {
        unlink(filepath); // Success, delete the file
        LOG_DEBUG("Relay worker: Deleted %s after successful delivery.",
                  filepath);
      } else {
        // Failed. Move to 'fail' directory or retry?
        // For now, log and leave it in the queue directory.
        // A more robust system would move it to a 'defer' or 'fail' directory.
        LOG_ERROR("Relay worker: Failed to relay %s. File remains in queue "
                  "directory.",
                  filepath);
      }
      free(filepath);
    }
  }
  LOG_INFO("Relay worker thread stopped.");
  return NULL;
}

int relay_init(config_t *config) {
  if (!config)
    return -1;
  g_config = config;

  // Create Queue
  g_work_queue = queue_create();
  if (!g_work_queue) {
    LOG_FATAL("Failed to create relay work queue");
    return -1;
  }

  // Count workers
  g_num_workers = config->upstream.relay_threads;
  if (g_num_workers <= 0)
    g_num_workers = 4; // Default to 4 workers

  LOG_INFO("Relay initialized with %d worker threads", g_num_workers);
  return 0;
}

void relay_start(void) {
  if (g_running)
    return;
  g_running = 1;

  // Start Workers
  g_worker_threads = calloc(g_num_workers, sizeof(pthread_t));
  if (!g_worker_threads) {
    LOG_FATAL("Failed to allocate memory for relay worker threads.");
    g_running = 0;
    return;
  }
  for (int i = 0; i < g_num_workers; i++) {
    pthread_create(&g_worker_threads[i], NULL, relay_worker_thread, NULL);
  }

  // Start Scanner
  pthread_create(&g_scanner_thread, NULL, relay_scanner_thread, NULL);

  LOG_INFO("Relay service started");
}

void relay_stop(void) {
  if (!g_running)
    return;
  g_running = 0;

  // Signal queue to stop blocking and allow threads to exit
  queue_stop(g_work_queue);

  // Join Scanner
  pthread_join(g_scanner_thread, NULL);

  // Join Workers
  for (int i = 0; i < g_num_workers; i++) {
    pthread_join(g_worker_threads[i], NULL);
  }

  free(g_worker_threads);
  queue_destroy(g_work_queue);
  g_work_queue = NULL; // Clear pointer after destruction

  LOG_INFO("Relay service stopped");
}
