#include "tlv.h"
#include <sstream>
#include <iomanip>

TLV::TLV(const ByteArray& tag, const ByteArray& value)
    : tag(tag), value(value) {}

std::string TLV::tagHex() const {
    std::ostringstream oss;
    for (uint8_t byte : tag) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

std::string TLV::valueHex() const {
    std::ostringstream oss;
    for (uint8_t byte : value) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

std::string TLV::lengthHex() const {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
        << static_cast<int>(value.size());
    return oss.str();
}
