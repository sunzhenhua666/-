#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stdint.h>

// Initialize a listening socket
// Returns fd on success, -1 on error
int create_tcp_server_socket(const char *addr, int port);

// Make a socket non-blocking
int make_socket_non_blocking(int fd);

// Set TCP_NODELAY (disable Nagle)
int set_tcp_nodelay(int fd);

#endif // SOCKET_UTILS_H
