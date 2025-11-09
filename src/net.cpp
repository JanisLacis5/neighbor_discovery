#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <sys/random.h>
#include <unistd.h>
#include "net.h"

bool is_eth(struct ifaddrs* ifa) {
    int family = ifa->ifa_addr->sa_family;
    if (family != AF_PACKET)
        return false;

    struct sockaddr_ll* flags = (struct sockaddr_ll*)ifa->ifa_addr;
    if (flags->sll_hatype != ARPHRD_ETHER)
        return false;

    char pathw[256] = {};
    char pathd[256] = {};
    snprintf(pathw, 255, "/sys/class/net/%s/wireless", ifa->ifa_name);

    // https://www.ibm.com/docs/en/linux-on-systems?topic=ni-matching-devices-1
    snprintf(pathd, 255, "/sys/class/net/%s/device",
             ifa->ifa_name);  // TODO: test this one (should check if device is not virtual)

    return access(pathw, F_OK) != 0 && access(pathd, F_OK) == 0;
}
