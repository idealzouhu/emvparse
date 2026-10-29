#ifndef EMVPARSE_HEX_UTIL_H
#define EMVPARSE_HEX_UTIL_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * Canonical hex→bytes for EmvParseCore (CLI/facade/APDU script). Strips whitespace;
 * requires even length and valid digits. Empty input → empty vector; invalid → empty vector.
 */
std::vector<uint8_t> parseHexString(const std::string& hexStr);

#endif // EMVPARSE_HEX_UTIL_H
