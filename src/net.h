#ifndef NET_H
#define NET_H

#include <ifaddrs.h>

bool is_eth(struct ifaddrs* ifa);

#endif
