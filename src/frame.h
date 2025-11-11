#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include "types.h"

void handle_frame(uint8_t* buf, ssize_t len);
void send_hello(int fd, int iface_idx, const uint8_t* ipv4, const uint8_t* ipv6, const uint8_t* source_mac);

#endif
