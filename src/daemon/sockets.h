#ifndef SOCKETS_H
#define SOCKETS_H

#include <cstdio>
#include <cstdint>

int open_socket(int iface_idx);
void close_sock(int iface_idx);
int add_to_epoll(int fd);
int scks_cleanup();
int read_full(int accfd, uint8_t* buf, size_t& len);

#endif
