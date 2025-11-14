#include <sys/socket.h>
#include <sys/un.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

constexpr int MAX_BUF_SIZE = 500;

void pack_buffer(char** args, int len, uint8_t* buf) {
    uint8_t* bufptr = buf;
    std::memcpy(bufptr, &len, 4);
    bufptr += 4;

    for (int i = 0; i < len; i++) {
        uint32_t arglen = sizeof(args[i]);

        std::memcpy(bufptr, &arglen, 4);
        bufptr += 4;

        std::memcpy(bufptr, args[i], arglen);
        bufptr += arglen;
    } 
}

int main(int argc, char** argv) {
    // TODO: open a new socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    char sock_path[] = "/tmp/neighbotdisc/cli.sock";
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, sock_path, sizeof(sock_path));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("main (connect)");
        return -1;
    }

    // TODO: process users input
    uint8_t buf[MAX_BUF_SIZE];
    pack_buffer(argv, argc, buf);

    // TODO: create buffer to send to the daemon

    // TODO: send the buffer and process the response

    // TODO: print pretty result to the stdout
    return 0;
}
