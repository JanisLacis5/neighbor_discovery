#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <net/if.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "constants.h"

constexpr int BUFSND_SIZE = 512;
constexpr int BUFRCV_SIZE = 8192;

void pack_buffer(char** argv, int argc, uint8_t* buf) {
    uint8_t* bufptr = buf;

    // Calculate buffer size in bytes
    uint32_t buflen = 4;  // argc
    for (int i = 1; i < argc; i++) {
        buflen += 4 + std::strlen(argv[i]);
    }

    std::memcpy(bufptr, &buflen, 4);
    bufptr += 4;

    std::memcpy(bufptr, &argc, 4);
    bufptr += 4;

    for (int i = 1; i < argc; i++) {
        uint32_t arglen = std::strlen(argv[i]);

        std::memcpy(bufptr, &arglen, 4);
        bufptr += 4;

        std::memcpy(bufptr, argv[i], arglen);
        bufptr += arglen;
    }
}

void write_buf(uint8_t* buf, uint16_t len) {
    ssize_t off = 0;
    while (off < len) {
        ssize_t m = write(STDOUT_FILENO, buf + off, len - off);
        if (m < 0) return;
        off += m;
    }

    if (len < 0) {
        perror("recv");
    }
}

int main(int argc, char** argv) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(CLI_SOCKET_PATH));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        return -1;
    }

    uint8_t bufsnd[BUFSND_SIZE];
    pack_buffer(argv, argc, bufsnd);

    ssize_t nsent = send(fd, bufsnd, BUFSND_SIZE, 0);
    if (nsent == -1) {
        perror("send");
        return -1;
    }

    uint8_t bufrcv[BUFRCV_SIZE];
    while (true) {
        ssize_t nrcv = recv(fd, bufrcv, BUFRCV_SIZE, 0);
        if (nrcv == 0)
            break;
        if (nrcv == -1) {
            perror("recv");
            return -1;
        }

        write_buf(bufrcv, nrcv);
    }

    return 0;
}
