#ifndef EMVPARSE_TLV_TREE_FORMATTER_H
#define EMVPARSE_TLV_TREE_FORMATTER_H

#include "tlv.h"
#include <memory>
#include <string>
#include <vector>

class TagRegistry;

/** JSON array output for parseHexToJson; attaches registry semantics per node. */
class TlvTreeFormatter {
public:
    static std::string toJsonArray(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry
    );
};

#endif // EMVPARSE_TLV_TREE_FORMATTER_H
