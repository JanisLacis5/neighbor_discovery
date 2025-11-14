#include <sys/epoll.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include "common.h"
#include "frame.h"
#include "ifaces.h"
#include "sockets.h"
#include "types.h"

constexpr int MAX_EVENTS = 10;
constexpr int CLI_REQ_SIZE = 512;
GlobalData gdata;
uint8_t cli_message[CLI_REQ_SIZE];
uint32_t cli_message_len = 0;

static int del_exp_devices() {
    std::vector<uint64_t> todel;
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0) {
        perror("del_exp_devices");
        return -1;
    }

    for (auto& [dev_id, device] : gdata.store) {
        if (curr_time - device.last_seen_ms > 30'000 || device.ifaces.empty()) {
            todel.push_back(dev_id);
        }
    }

    for (uint64_t id : todel)
        gdata.store.erase(id);
    return 0;
}

void handle_cli_tokens(std::vector<std::string>& tokens) {
    for (std::string& token : tokens)
        std::cout << token << std::endl;
}

// total_len_in_bytes(4) | total_len(4) | tlen(4) | token(tlen) | tlen(4) | token(tlen) ...
void handle_cli_req() {
    if (cli_message_len < 4)
        return;
    uint8_t* buf = cli_message;

    // Read the length of the buffer
    uint32_t totallen;
    std::memcpy(&totallen, buf, 4);
    buf += 4;

    if (totallen > cli_message_len)
        return;

    uint32_t tokens_cnt;
    std::memcpy(&tokens_cnt, buf, 4);
    buf += 4;

    // If all tokens have been received, process them
    std::vector<std::string> tokens(tokens_cnt);
    for (int i = 0; i < tokens_cnt; i++) {
        uint32_t tlen;

        std::memcpy(&tlen, buf, 4);
        buf += 4;

        tokens[i].resize(tlen);
        std::memcpy(tokens[i].data(), buf, tlen);
        buf += tlen;
    }

    handle_cli_tokens(tokens);
    cli_message_len = 0;
}

int main() {
    // Set the device id
    int err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        perror("main (random num generation)");
        return -1;
    }

    // Open a listening socket for the cli
    int cli_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    struct sockaddr_un addr;
    char sock_path[] = "/tmp/neighbordiscoverycli.sock";
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, sock_path, sizeof(sock_path));

    err = unlink(sock_path);
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
                int accfd = accept4(cli_fd, nullptr, nullptr, SOCK_NONBLOCK);
                if (accfd == -1) {
                    perror("main (accept)");
                    return -1;
                }

                while (true) {
                    ssize_t recvlen = recv(accfd, cli_message + cli_message_len, CLI_REQ_SIZE - cli_message_len, 0);

                    if (recvlen == 0)
                        break;
                    if (recvlen == -1) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                            perror("main (reading cli socket)");
                        break;
                    }

                    cli_message_len += recvlen;
                }
                handle_cli_req();
            }
            else {
                uint8_t buf[1500];
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
        for (int idx = 1; idx < gdata.sockets.size(); idx++) {
            SocketInfo& sock_info = gdata.sockets[idx];
            const IfaceInfo& iface_info = gdata.idx_to_info[idx];

            if (sock_info.fd == -1)
                continue;
            if (curr_time - sock_info.last_sent_ms < 5'000)
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
