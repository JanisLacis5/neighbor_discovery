#include "cli_tokens.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "cli_commands.h"

void handle_tokens(int fd, std::vector<std::string>& tokens) {
    if (tokens.size() == 0)
        printf("No tokens received from cli\n");
    else if (tokens[0] == "list")
        cli_listall(fd);
}

// total_len_in_bytes(4) | total_len(4) | tlen(4) | token(tlen) | tlen(4) | token(tlen) ...
void read_tokens(uint8_t* buf, size_t len, std::vector<std::string>& tokens) {
    if (len < 4)
        return;

    // Read the length of the buffer
    uint32_t totallen;
    std::memcpy(&totallen, buf, 4);
    buf += 4;

    if (totallen != len)
        return;

    uint32_t tokens_cnt;
    std::memcpy(&tokens_cnt, buf, 4);
    buf += 4;

    // If all tokens have been received, process them
    tokens.resize(tokens_cnt);
    for (uint32_t i = 0; i < tokens_cnt; i++) {
        uint32_t tlen;

        std::memcpy(&tlen, buf, 4);
        buf += 4;

        tokens[i].resize(tlen);
        std::memcpy(tokens[i].data(), buf, tlen);
        buf += tlen;
    }
}


