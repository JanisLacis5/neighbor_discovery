#ifndef SOCKETS_H
#define SOCKETS_H

int open_socket(int iface_idx);
void close_sock(int iface_idx);
int scks_cleanup();

#endif
