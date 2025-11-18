#include "cli_commands.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include "types.h"

constexpr size_t BUF_SIZE = 8192;  // 8kb
constexpr size_t TMP_LINE_SIZE = 256;
static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static bool all_zeroes(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != 0)
            return false;
    }
    return true;
}

static void format_devid(uint64_t n, char out[27]) {
    char tmp[16];
    int i = 0;

    while (i < 13) {
        tmp[12 - i] = alphabet[n & 0x1F];
        n >>= 5;
        i++;
    }

    snprintf(out, 27, "%.4s-%.4s-%.4s-%c", tmp, tmp + 4, tmp + 8, tmp[12]);
}

static int send_mes(int fd, char* buf, size_t& len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("send_mes");
            return -1;
        }
        if (n == 0)
            break;

        sent += n;
    }

    len = 0;
    return 0;
}

static int checkn(int n, char* buf, size_t& pos, int fd, const char* func_name) {
    if (n < 0 || n > BUF_SIZE) {
        perror(func_name);
        return -1;
    }

    if (n + pos >= BUF_SIZE) {
        if (send_mes(fd, buf, pos) == -1)
            return -1;
    }
    return 0;
}

static int add_device_id(uint64_t devid, char* buf, size_t& pos, int fd) {
    char idstr[27];
    format_devid(devid, idstr);

    char line[TMP_LINE_SIZE];
    int n = std::sprintf(line, "DEVICE ID: %s\n", idstr);

    if (checkn(n, buf, pos, fd, "add_device_id") == -1)
        return -1;

    std::memcpy(buf + pos, line, n);
    pos += n;
    return 0;
}

static int add_iface_name(int iface_idx, char* buf, size_t& pos, int fd) {
    char iface_name[IF_NAMESIZE];
    if (!if_indextoname(iface_idx, iface_name)) {
        perror("add_iface_name1");
        return -1;
    }

    char line[TMP_LINE_SIZE];
    int n = std::sprintf(line, "\t%s\n", iface_name);

    if (checkn(n, buf, pos, fd, "add_iface_name") == -1)
        return -1;

    std::memcpy(buf + pos, line, n);
    pos += n;
    return 0;
}

static int add_mac(const uint8_t* mac, char* buf, size_t& pos, int fd) {
    char line[TMP_LINE_SIZE];

    const uint8_t* m = mac;
    int n = std::sprintf(line, "\t\tMAC: %02x:%02x:%02x:%02x:%02x:%02x\n", m[0], m[1], m[2], m[3], m[4], m[5]);

    if (checkn(n, buf, pos, fd, "add_mac") == -1)
        return -1;

    std::memcpy(buf + pos, line, n);
    pos += n;
    return 0;
}

static int add_ip(sa_family_t family, const uint8_t* ip, char* buf, size_t& pos, int fd) {
    char line[TMP_LINE_SIZE];
    int n;

    if (all_zeroes(ip, (family == AF_INET6 ? IPV6_LEN : IPV4_LEN)))
        n = std::sprintf(line, "\t\t%s: NONE\n", (family == AF_INET6 ? "IPv6" : "IPv4"));

    else if (family == AF_INET6) {
        char ip6str[INET6_ADDRSTRLEN];
        if (!inet_ntop(AF_INET6, ip, ip6str, sizeof(ip6str))) {
            perror("inet_ntop IPv6");
            return -1;
        }
        n = std::sprintf(line, "\t\tIPv6: %s\n", ip6str);
    }

    else if (family == AF_INET) {
        char ip4str[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, ip, ip4str, sizeof(ip4str))) {
            perror("inet_ntop IPv4");
            return -1;
        }
        n = std::sprintf(line, "\t\tIPv4: %s\n", ip4str);
    }
    else
        return -1;

    if (checkn(n, buf, pos, fd, "add_ip") == -1)
        return -1;

    std::memcpy(buf + pos, line, n);
    pos += n;

    return 0;
}

void cli_listall(int fd) {
    if (gdata.store.empty()) {
        char buf[] = "No neighbors found\n";
        if (send(fd, buf, sizeof(buf), 0) == -1)
            perror("cli_listall (empty)");

        return;
    }

    char buf[BUF_SIZE];
    size_t pos = 0;

    for (auto& [devid, device] : gdata.store) {
        if (devid == gdata.device_id)
            continue;

        if (add_device_id(devid, buf, pos, fd) == -1)
            return;

        for (auto& [iface_idx, iface_info] : device.ifaces) {
            if (add_iface_name(iface_idx, buf, pos, fd) == -1)
                return;

            if (add_mac(iface_info.mac, buf, pos, fd) == -1)
                return;

            if (add_ip(AF_INET, iface_info.ipv4, buf, pos, fd) == -1)
                return;

            if (add_ip(AF_INET6, iface_info.ipv6, buf, pos, fd) == -1)
                return;
        }
    }

    if (pos != 0) {
        if (send_mes(fd, buf, pos) == -1)
            perror("cli_listall");
    }
}
