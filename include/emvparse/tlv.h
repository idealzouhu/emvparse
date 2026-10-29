#ifndef TLV_H
#define TLV_H

#include <vector>
#include <string>
#include <memory>
#include "emvparse/emvparse_global.h"

class TLVParser;

class EMVPARSE_EXPORT TLV {
    friend class TLVParser;

public:
    using ByteArray = std::vector<uint8_t>;
    using Children = std::vector<std::shared_ptr<TLV>>;

    TLV(const ByteArray& tag, const ByteArray& value);

    const ByteArray& getTag() const { return tag; }
    const ByteArray& getValue() const { return value; }
    const Children& getChildren() const { return children; }

    std::string tagHex() const;
    std::string valueHex() const;
    std::string lengthHex() const;

private:
    ByteArray tag;
    ByteArray value;
    Children children;
};

#endif // TLV_H
