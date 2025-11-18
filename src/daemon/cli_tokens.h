#ifndef CLI_TOKENS_H
#define CLI_TOKENS_H

#include <cstdint>
#include <vector>
#include <string>

void handle_tokens(int fd, std::vector<std::string>& tokens);
void read_tokens(uint8_t* buf, size_t len, std::vector<std::string>& tokens);

#endif
