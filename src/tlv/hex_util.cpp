#include "hex_util.h"
#include <algorithm>
#include <cctype>

std::vector<uint8_t> parseHexString(const std::string& hexStr) {
    std::vector<uint8_t> data;

    std::string cleanHex = hexStr;
    cleanHex.erase(
        std::remove_if(cleanHex.begin(), cleanHex.end(), ::isspace),
        cleanHex.end());

    if (cleanHex.length() % 2 != 0) {
        return {};
    }

    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        const std::string byteStr = cleanHex.substr(i, 2);
        try {
            const uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            data.push_back(byte);
        } catch (const std::exception&) {
            return {};
        }
    }

    return data;
}
