#include "net.h"
#include <ifaddrs.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include "common.h"
#include "types.h"

static int open_socket(int ifa_idx) {
    // Open a new socket
    int fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_PROTOCOL));
    if (fd == -1) {
        printf("Error opening new socket\n");
        return -1;
    }

    // Bind it to the interface at index ifa_idx
    struct sockaddr_ll addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_PROTOCOL);
    addr.sll_ifindex = ifa_idx;
    int err = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err) {
        close(fd);
        printf("Error binding socket to interface with index '%d'\n", ifa_idx);
        return -1;
    }

    // Add this socket to the global epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(gdata.epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        close(fd);
        printf("Error in epoll_ctl:add\n");
        return -1;
    }

    return fd;
}

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

int process_eth(struct ifaddrs* ifa) {
    int ifa_idx = if_nametoindex(ifa->ifa_name);
    uint64_t curr_time = get_curr_ms();

    if (gdata.sockets.find(ifa_idx) != gdata.sockets.end())
        gdata.sockets[ifa_idx].last_seen_ms = curr_time;
    else {
        int fd = open_socket(ifa_idx);
        if (fd < 0) {
            return -1;
        }

        struct SocketInfo sock_info;
        sock_info.fd = fd;
        sock_info.last_seen_ms = curr_time;
        sock_info.last_sent_ms = 0;
        gdata.sockets[ifa_idx] = sock_info;
    }

    return 0;
}
