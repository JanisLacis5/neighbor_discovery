#include "net.h"
#include <ifaddrs.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
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

    // Add socket to the global fd -> interface map
    if (gdata.fd_to_iface.size() <= fd)
        gdata.fd_to_iface.resize(fd + 1);
    gdata.fd_to_iface[fd] = ifa_idx;

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

static bool all_zero(uint8_t* num, size_t len) {
    return std::all_of(num, num + len, [](uint8_t x) { return x == 0; });
}

static int update_ifinfo(struct ifaddrs* ifa) {
    int64_t curr_time = get_curr_ms();
    int family = ifa->ifa_addr->sa_family;

    int ifa_idx = if_nametoindex(ifa->ifa_name);
    if (ifa_idx == 0) {
        perror("if_nametoindex failed\n");
        return -1;
    }

    if (gdata.idx_to_info.size() <= ifa_idx)
        gdata.idx_to_info.resize(ifa_idx + 1);

    struct IfaceInfo& if_info = gdata.idx_to_info[ifa_idx];
    if_info.last_seen_ms = curr_time;

    if (family == AF_PACKET && !if_info.is_init) {
        struct sockaddr_ll* sll = (struct sockaddr_ll*)ifa->ifa_addr;
        std::memcpy(if_info.iface_name, &ifa->ifa_name, sizeof(ifa->ifa_name));
        std::memcpy(if_info.mac, sll->sll_addr, 8);
        if_info.is_init = true;
    }
    else if (family == AF_INET && all_zero(&if_info.ipv4[0], 4)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
        std::memcpy(if_info.ipv4, &sin->sin_addr.s_addr, 4);
    }
    else if (family == AF_INET6 && all_zero(&if_info.ipv6[0], 16)) {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
        std::memcpy(if_info.ipv6, sin6->sin6_addr.s6_addr, 16);
    }
    else
        // This function should only receive AF_PACKET, AF_INET or AF_INET6 interfaces
        return -1;

    return 0;
}

int process_if(struct ifaddrs* ifa) {
    int ifa_idx = if_nametoindex(ifa->ifa_name);
    int64_t curr_time = get_curr_ms();

    // Update interface info
    if (update_ifinfo(ifa) == -1)
        return -1;

    // Update socket info for the interface
    if (gdata.sockets[ifa_idx].fd != -1)
        gdata.sockets[ifa_idx].last_seen_ms = curr_time;
    else {
        int fd = open_socket(ifa_idx);
        if (fd < 0) {
            return -1;
        }

        if (gdata.sockets.size() <= ifa_idx)
            gdata.sockets.resize(ifa_idx + 1);
        struct SocketInfo sock_info = gdata.sockets[ifa_idx];
        sock_info.fd = fd;
        sock_info.last_seen_ms = curr_time;
        sock_info.last_sent_ms = 0;
    }

    return 0;
}
