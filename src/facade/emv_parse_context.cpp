#include "emv_parse_context.h"
#include "tag_registry.h"
#include "tlv_parser.h"

TagRegistry* EmvParseContext::tagRegistry() const {
    return tlvParser.tagRegistry();
}
