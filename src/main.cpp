#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>

int main() {
    struct ifaddrs* ifaddr;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        printf("error\n");
        return -1;
    }

    while (ifaddr != NULL) {
        if (ifaddr->ifa_addr == NULL)
            continue;

        int family = ifaddr->ifa_addr->sa_family;

        ifaddr = ifaddr->ifa_next;
    }
    return 0;
}
