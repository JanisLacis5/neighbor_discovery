#ifndef SOCKETS_H
#define SOCKETS_H

int open_socket(int iface_idx);
void close_sock(int iface_idx);
int add_to_epoll(int fd);
int scks_cleanup();

#endif
