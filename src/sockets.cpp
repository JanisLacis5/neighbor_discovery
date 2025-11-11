#include "sockets.h"
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include "common.h"
#include "types.h"

// TODO: split into multiple
int open_socket(int iface_idx) {
    // Open a new socket
    int fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_PROTOCOL));
    if (fd == -1) {
        perror("open_socket (socket)");
        return -1;
    }

    // Bind it to the interface at index ifa_idx
    struct sockaddr_ll addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_PROTOCOL);
    addr.sll_ifindex = iface_idx;
    int err = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err) {
        close(fd);
        perror("open_socket (bind)");
        return -1;
    }

    // Add this socket to the global epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(gdata.epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        close(fd);
        perror("open_socket (epoll_ctl)");
        return -1;
    }

    // Add socket to the global fd -> interface map
    if (gdata.fd_to_iface.size() <= fd)
        gdata.fd_to_iface.resize(fd + 1);
    gdata.fd_to_iface[fd] = iface_idx;

    return fd;
}

void close_sock(int iface_idx) {
    if (iface_idx <= 0 || iface_idx >= gdata.fd_to_iface.size())
        return;

    int fd = gdata.sockets[iface_idx].fd;
    if (fd < 0)
        return;

    close(fd);
    gdata.sockets[iface_idx].fd = -1;
    gdata.fd_to_iface[fd] = -1;
}

int scks_cleanup() {
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0)
        return -1;

    std::vector<int> todel;

    for (int idx = 0; idx < gdata.sockets.size(); idx++) {
        SocketInfo& sock_info = gdata.sockets[idx];
        if (sock_info.fd == -1 ||
            curr_time - sock_info.last_seen_ms < 15'000)  // filter sockets that are idle for 15 seconds or more
            continue;

        // Remove socket from epoll
        if (epoll_ctl(gdata.epollfd, EPOLL_CTL_DEL, sock_info.fd, NULL) == -1) {
            perror("scks_cleanup");
            return -1;
        }

        todel.push_back(idx);
    }
    for (int idx : todel) {
        close_sock(idx);
    }
    return 0;
}
