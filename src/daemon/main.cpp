#include <sys/epoll.h>
#include <sys/random.h>
#include <sys/un.h>
#include <csignal>
#include <cstring>
#include <string>
#include "cli_tokens.h"
#include "common.h"
#include "constants.h"
#include "frame.h"
#include "ifaces.h"
#include "sockets.h"
#include "types.h"

constexpr int MAX_EVENTS = 10;
constexpr int CLI_REQ_SIZE = 512;
GlobalData gdata;

static int del_exp_devices() {
    std::vector<uint64_t> todel;
    int64_t curr_time = get_curr_ms();

    for (auto& [dev_id, device] : gdata.store) {
        if (curr_time - device.last_seen_ms > DEVICE_EXP_MS || device.ifaces.empty())
            todel.push_back(dev_id);
    }

    for (uint64_t id : todel)
        gdata.store.erase(id);
    return 0;
}

int main() {
    // Set the device id
    uint8_t tmp[DEVICE_ID_LEN];
    int err = getrandom(tmp, DEVICE_ID_LEN, GRND_RANDOM);
    if (err <= 0) {
        perror("main (random num generation)");
        return -1;
    }
    std::memcpy(&gdata.device_id, tmp, DEVICE_ID_LEN);

    // Open a listening socket for the cli
    int cli_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(CLI_SOCKET_PATH));

    err = unlink(CLI_SOCKET_PATH);
    if (err == -1 && errno != ENOENT) {
        perror("main (unlink)");
        return -1;
    }

    err = bind(cli_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un));
    if (err == -1) {
        perror("main (cli socket bind)");
        return -1;
    }

    err = listen(cli_fd, SOMAXCONN);
    if (err == -1) {
        perror("main (cli socket listen)");
        return -1;
    }

    // Create the epoll
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("main (epoll_create");
        return -1;
    }
    gdata.epollfd = epollfd;
    struct epoll_event events[MAX_EVENTS];

    // Add the cli socket to the epoll
    if (add_to_epoll(cli_fd) == -1)
        return -1;

    while (true) {
        err = ifaces_refresh();
        if (err < 0)
            return -1;

        err = scks_cleanup();
        if (err < 0)
            return -1;

        int nfds = epoll_wait(gdata.epollfd, events, MAX_EVENTS, 1000);
        if (nfds == -1) {
            perror("main (epoll_wait)");
            return -1;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == cli_fd) {
                std::vector<std::string> tokens;
                uint8_t buf[CLI_REQ_SIZE];
                size_t len = 0;

                int accfd = accept(fd, nullptr, nullptr);
                if (accfd == -1) {
                    perror("read_main_buf (accept)");
                    return -1;
                }

                if (read_full(accfd, buf, len) == -1) {
                    perror("read_main_buf (read_raw_buf)");
                    return -1;
                }
                read_tokens(buf, len, tokens);
                handle_tokens(accfd, tokens);
                close(accfd);
            }
            else {
                uint8_t buf[8194];
                while (true) {
                    ssize_t recvlen = recvfrom(fd, buf, sizeof(buf), 0, NULL, 0);

                    if (recvlen == 0)
                        break;
                    if (recvlen == -1) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                            perror("main (reading socket)");
                        break;
                    }

                    int iface_idx = gdata.fd_to_iface[fd];
                    handle_frame(iface_idx, buf, recvlen);
                }
            }
        }

        int64_t curr_time = get_curr_ms();
        for (size_t idx = 1; idx < gdata.sockets.size(); idx++) {
            SocketInfo& sock_info = gdata.sockets[idx];
            const IfaceInfo& iface_info = gdata.idx_to_info[idx];

            if (sock_info.fd == -1)
                continue;
            if (curr_time - sock_info.last_sent_ms < HELLO_INTERVAL)
                continue;

            send_hello(sock_info.fd, idx, iface_info.ipv4, iface_info.ipv6, iface_info.mac);
            sock_info.last_sent_ms = curr_time;
        }

        err = del_exp_devices();
        if (err < 0)
            return -1;

        err = del_exp_ifaces();
        if (err < 0)
            return -1;
    }

    return 0;
}
