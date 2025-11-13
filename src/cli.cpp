#include <sys/socket.h>

int main(int argc, char** argv) {
    // TODO: open a new socket
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // TODO: process users input

    // TODO: create buffer to send to the daemon

    // TODO: send the buffer and process the response

    // TODO: print pretty result to the stdout
    return 0;
}
