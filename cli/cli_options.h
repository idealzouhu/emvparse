#ifndef EMVPARSE_CLI_OPTIONS_H
#define EMVPARSE_CLI_OPTIONS_H

#include <cstring>

inline bool isHelpOption(const char* arg) {
    return std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0;
}

inline bool isRegistryPathOption(const char* arg) {
    return std::strcmp(arg, "--registry") == 0 || std::strcmp(arg, "--manifest") == 0;
}

inline bool isOutputPathOption(const char* arg) {
    return std::strcmp(arg, "--output") == 0 || std::strcmp(arg, "-o") == 0;
}

#endif
