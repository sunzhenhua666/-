#include "socket_utils.h"
#include "logger.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


int create_tcp_server_socket(const char *addr, int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    LOG_ERROR("socket() failed: %s", strerror(errno));
    return -1;
  }

  // Reuse Address
  int on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    LOG_WARN("setsockopt(SO_REUSEADDR) failed");
  }

  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  if (inet_pton(AF_INET, addr, &saddr.sin_addr) <= 0) {
    LOG_ERROR("Invalid bind address: %s", addr);
    close(fd);
    return -1;
  }

  if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
    LOG_ERROR("bind() failed: %s", strerror(errno));
    close(fd);
    return -1;
  }

  if (listen(fd, 1024) == -1) {
    LOG_ERROR("listen() failed: %s", strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}

int make_socket_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR("fcntl(F_GETFL) failed");
    return -1;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    LOG_ERROR("fcntl(F_SETFL) failed");
    return -1;
  }
  return 0;
}

int set_tcp_nodelay(int fd) {
  int on = 1;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == -1) {
    LOG_WARN("setsockopt(TCP_NODELAY) failed");
    return -1;
  }
  return 0;
}
