#include "tlv_tree_formatter.h"
#include "tlv_formatter.h"
#include "tag_registry.h"
#include "tlv_semantic_for_tlv.h"
#include "tlv_semantic_summary.h"
#include <emvparse/output_options.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

namespace {

std::string indentSpaces(int depth) {
    return std::string(static_cast<size_t>(depth * 2), ' ');
}

std::string treeValueLengthHex(std::size_t length) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    if (length <= 0xFF) {
        oss << std::setw(2) << length;
    } else if (length <= 0xFFFF) {
        oss << std::setw(4) << length;
    } else {
        oss << length;
    }
    return oss.str();
}

size_t treeTagNameColumn(int depth, size_t tagWidth, size_t lenWidth) {
    return indentSpaces(depth).size() + tagWidth + 1 + lenWidth + 4;
}

void appendTreeSiblingGroup(
    const std::vector<std::shared_ptr<TLV>>& nodes,
    TagRegistry* registry,
    int depth,
    const emvparse::FormatOptions& options,
    std::ostringstream& out
);

void appendTreeOneLine(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth,
    size_t tagWidth,
    size_t lenWidth,
    const emvparse::FormatOptions& options,
    std::ostringstream& out
) {
    const std::string tagHex = tlv->tagHex();
    std::shared_ptr<TagInfo> tagInfo = registry ? registry->getTagInfo(tagHex) : nullptr;
    const bool composite = !tlv->getChildren().empty();
    const std::string name = tagInfo ? tagInfo->name : "";
    const std::string lenHex = treeValueLengthHex(tlv->getValue().size());

    out << indentSpaces(depth) << std::left << std::setfill(' ') << std::setw(static_cast<int>(tagWidth))
        << tagHex << " " << std::setw(static_cast<int>(lenWidth)) << lenHex;
    if (!name.empty()) {
        out << " -> " << name;
    }

    const bool showValueHex =
        options.tree == emvparse::TreeStyle::Verbose && !composite &&
        (!tagInfo || tagInfo->type != "LIST");
    if (showValueHex) {
        const std::string& hex = tlv->valueHex();
        if (!hex.empty()) {
            out << "  " << hex;
        }
    }
    out << "\n";

    if (!composite && tagInfo) {
        const auto semantic = semanticForTlv(tlv, registry, tagInfo);
        const auto mode = effectiveSemanticSummaryMode(tagInfo->type, options.semantic);
        const auto lines = buildTreeSemanticSummaryLines(semantic, mode);
        const size_t nameColumn = treeTagNameColumn(depth, tagWidth, lenWidth);
        for (const auto& line : lines) {
            out << std::string(nameColumn, ' ') << line.text << "\n";
        }
    }
}

void appendTreeSiblingGroup(
    const std::vector<std::shared_ptr<TLV>>& nodes,
    TagRegistry* registry,
    int depth,
    const emvparse::FormatOptions& options,
    std::ostringstream& out
) {
    if (nodes.empty()) {
        return;
    }

    size_t tagWidth = 2;
    size_t lenWidth = 2;
    for (const auto& tlv : nodes) {
        tagWidth = std::max(tagWidth, tlv->tagHex().size());
        lenWidth = std::max(lenWidth, treeValueLengthHex(tlv->getValue().size()).size());
    }

    for (const auto& tlv : nodes) {
        appendTreeOneLine(tlv, registry, depth, tagWidth, lenWidth, options, out);
        const auto& children = tlv->getChildren();
        if (!children.empty()) {
            appendTreeSiblingGroup(children, registry, depth + 1, options, out);
        }
    }
}

} // namespace

std::string TlvTreeFormatter::toJsonArray(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry
) {
    nlohmann::ordered_json jsonArray = nlohmann::ordered_json::array();
    for (const auto& tlv : roots) {
        jsonArray.push_back(TLVFormatter::toJson(tlv, registry));
    }
    return jsonArray.dump(2);
}

std::string TLVFormatter::toTreeText(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream out;
    appendTreeSiblingGroup(roots, registry, 0, options, out);
    return out.str();
}
