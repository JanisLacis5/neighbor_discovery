#include <sys/socket.h>
#include <cstdint>
#include <cstring>

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

    // TODO: process users input
    uint8_t buf[MAX_BUF_SIZE];
    pack_buffer(argv, argc, buf);

    // TODO: create buffer to send to the daemon

    // TODO: send the buffer and process the response

    // TODO: print pretty result to the stdout
    return 0;
}
