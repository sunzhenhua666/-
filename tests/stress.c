#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2525
#define THREADS 10
#define EMAILS_PER_THREAD 100

static int g_success_count = 0;
static int g_fail_count = 0;
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void *worker_thread(void *arg) {
  (void)arg;
  char buf[4096];
  struct sockaddr_in serv_addr;

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

  for (int i = 0; i < EMAILS_PER_THREAD; i++) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      perror("Socket Connect Error");
      pthread_mutex_lock(&g_stats_mutex);
      g_fail_count++;
      pthread_mutex_unlock(&g_stats_mutex);
      continue;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      // perror("Connect Failed");
      close(sock);
      pthread_mutex_lock(&g_stats_mutex);
      g_fail_count++;
      pthread_mutex_unlock(&g_stats_mutex);
      continue;
    }

    // Read Greeting
    read(sock, buf, sizeof(buf));

    // EHLO
    write(sock, "EHLO stress.test\r\n", 18);
    read(sock, buf, sizeof(buf));

    // MAIL FROM
    write(sock, "MAIL FROM: <stress@test.com>\r\n", 30);
    read(sock, buf, sizeof(buf));

    // RCPT TO
    write(sock, "RCPT TO: <victim@example.com>\r\n", 31);
    read(sock, buf, sizeof(buf));

    // DATA
    write(sock, "DATA\r\n", 6);
    read(sock, buf, sizeof(buf));

    // Body
    char body[1024];
    snprintf(body, sizeof(body),
             "Subject: Stress Test %d\r\n"
             "\r\n"
             "This is a stress test email %d from thread %lu.\r\n"
             ".\r\n",
             i, i, (unsigned long)pthread_self());
    write(sock, body, strlen(body));
    read(sock, buf, sizeof(buf));

    // QUIT
    write(sock, "QUIT\r\n", 6);
    // read(sock, buf, sizeof(buf));

    close(sock);

    pthread_mutex_lock(&g_stats_mutex);
    g_success_count++;
    pthread_mutex_unlock(&g_stats_mutex);
  }
  return NULL;
}

double get_time_sec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main(int argc, char *argv[]) {
  int threads = THREADS;
  if (argc > 1)
    threads = atoi(argv[1]);

  printf("Starting stress test with %d threads, %d emails each...\n", threads,
         EMAILS_PER_THREAD);

  pthread_t *t = calloc(threads, sizeof(pthread_t));

  double start = get_time_sec();

  for (int i = 0; i < threads; i++) {
    pthread_create(&t[i], NULL, worker_thread, NULL);
  }

  for (int i = 0; i < threads; i++) {
    pthread_join(t[i], NULL);
  }

  double end = get_time_sec();
  double duration = end - start;

  printf("Test finished in %.2f seconds.\n", duration);
  printf("Total Success: %d\n", g_success_count);
  printf("Total Failed: %d\n", g_fail_count);
  printf("Throughput: %.2f emails/sec\n",
         (g_success_count + g_fail_count) / duration);

  free(t);
  return 0;
}
