#include "tlv_formatter.h"
#include "tag_registry.h"
#include "tlv_semantic_summary.h"
#include "value_semantic_parser.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <vector>
#include <algorithm>

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

std::string spacedHexUpper(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i < hex.size(); i += 2) {
        if (i > 0) {
            out += ' ';
        }
        if (i + 1 < hex.size()) {
            out += hex[i];
            out += hex[i + 1];
        } else {
            out += hex[i];
        }
    }
    return out;
}

std::string htmlEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

nlohmann::ordered_json semanticForTlv(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    const std::shared_ptr<TagInfo>& tagInfo
) {
    if (!tagInfo) {
        return nlohmann::ordered_json::object();
    }
    if (!tlv->getChildren().empty()) {
        nlohmann::ordered_json semantic;
        semantic["raw"] = tlv->valueHex();
        return semantic;
    }
    auto semantic = ValueSemanticParser::parseValueSemantic(tlv, tagInfo);
    if (registry && tagInfo->type == "LIST" && semantic.contains("dolEntries") &&
        semantic["dolEntries"].is_array()) {
        for (auto& entry : semantic["dolEntries"]) {
            if (!entry.is_object() || !entry.contains("tag")) {
                continue;
            }
            const std::string tag = entry["tag"].get<std::string>();
            if (auto info = registry->getTagInfo(tag)) {
                entry["name"] = info->name;
            }
        }
    }
    return semantic;
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
    if (!composite) {
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

void appendTreeLines(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth,
    const emvparse::FormatOptions& options,
    std::ostringstream& out
) {
    appendTreeSiblingGroup({tlv}, registry, depth, options, out);
}

void collectTableRows(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth,
    const emvparse::FormatOptions& options,
    std::vector<TableTextRow>& rows
) {
    const std::string tagHex = tlv->tagHex();
    std::shared_ptr<TagInfo> tagInfo = registry ? registry->getTagInfo(tagHex) : nullptr;
    const bool composite = !tlv->getChildren().empty();

    TableTextRow dataRow;
    dataRow.kind = TableTextRow::Kind::Data;
    dataRow.depth = depth;
    dataRow.tagHex = tagHex;
    dataRow.name = tagInfo ? tagInfo->name : "";
    dataRow.length = std::to_string(tlv->getValue().size());
    if (!composite) {
        dataRow.value = spacedHexUpper(tlv->valueHex());
    }
    rows.push_back(dataRow);

    if (!composite && tagInfo) {
        const auto semantic = semanticForTlv(tlv, registry, tagInfo);
        const auto mode = effectiveSemanticSummaryMode(tagInfo->type, options.semantic);
        const auto lines = buildSemanticSummaryLines(semantic, mode);
        for (const auto& line : lines) {
            TableTextRow semRow;
            semRow.kind = TableTextRow::Kind::Semantic;
            semRow.depth = depth;
            semRow.semanticText = line.text;
            rows.push_back(semRow);
        }
    }

    for (const auto& child : tlv->getChildren()) {
        collectTableRows(child, registry, depth + 1, options, rows);
    }
}

std::string formatTagColumn(int depth, const std::string& tagHex) {
    return indentSpaces(depth) + tagHex;
}

void renderFixedWidthTable(const std::vector<TableTextRow>& rows, std::ostringstream& out) {
    size_t tagWidth = 3;
    size_t nameWidth = 4;
    size_t lenWidth = 6;

    for (const auto& row : rows) {
        if (row.kind == TableTextRow::Kind::Data) {
            tagWidth = std::max(tagWidth, formatTagColumn(row.depth, row.tagHex).size());
            nameWidth = std::max(nameWidth, row.name.size());
            lenWidth = std::max(lenWidth, row.length.size());
        }
    }

    tagWidth = std::max(tagWidth, static_cast<size_t>(3));
    nameWidth = std::max(nameWidth, static_cast<size_t>(4));
    lenWidth = std::max(lenWidth, static_cast<size_t>(6));

    const size_t valueColumnStart = tagWidth + 2 + nameWidth + 2 + lenWidth + 2;

    auto writeHeader = [&]() {
        out << std::left << std::setw(static_cast<int>(tagWidth)) << "TAG" << "  "
            << std::setw(static_cast<int>(nameWidth)) << "Name" << "  "
            << std::right << std::setw(static_cast<int>(lenWidth)) << "Length" << "  "
            << std::left << "Value"
            << "\n";
    };

    if (rows.empty()) {
        return;
    }

    writeHeader();

    for (const auto& row : rows) {
        if (row.kind == TableTextRow::Kind::Data) {
            out << std::left << std::setw(static_cast<int>(tagWidth)) << formatTagColumn(row.depth, row.tagHex)
                << "  " << std::setw(static_cast<int>(nameWidth)) << row.name << "  "
                << std::right << std::setw(static_cast<int>(lenWidth)) << row.length << "  "
                << std::left << row.value << "\n";
        } else {
            out << std::string(valueColumnStart, ' ') << row.semanticText << "\n";
        }
    }
}

int g_htmlSemanticCounter = 0;

int maxDepthBelow(const std::shared_ptr<TLV>& tlv, int depth) {
    int maxD = depth;
    for (const auto& child : tlv->getChildren()) {
        maxD = std::max(maxD, maxDepthBelow(child, depth + 1));
    }
    return maxD;
}

int computeMaxTlvDepth(const std::vector<std::shared_ptr<TLV>>& roots) {
    int maxD = 0;
    for (const auto& root : roots) {
        maxD = std::max(maxD, maxDepthBelow(root, 0));
    }
    return maxD;
}

bool semanticLineRedundantWithHex(
    const std::string& text,
    const std::string& hexDisplay,
    const std::string& rawHex
) {
    if (text.empty()) {
        return true;
    }
    if (text == hexDisplay || text == rawHex) {
        return true;
    }
    std::string textNoSpace;
    textNoSpace.reserve(text.size());
    for (char c : text) {
        if (c != ' ') {
            textNoSpace += c;
        }
    }
    std::string hexNoSpace;
    hexNoSpace.reserve(hexDisplay.size());
    for (char c : hexDisplay) {
        if (c != ' ') {
            hexNoSpace += c;
        }
    }
    return !textNoSpace.empty() && textNoSpace == hexNoSpace;
}

void appendHtmlInlineSemanticLines(
    std::ostringstream& body,
    const std::vector<SemanticSummaryLine>& lines,
    const std::string& hexDisplay,
    const std::string& rawHex,
    bool styleInactiveBits
) {
    for (const auto& line : lines) {
        if (semanticLineRedundantWithHex(line.text, hexDisplay, rawHex)) {
            continue;
        }
        body << "<br>";
        if (styleInactiveBits && line.inactive) {
            body << "<span class=\"inactive-bit\">" << htmlEscape(line.text) << "</span>";
        } else {
            body << htmlEscape(line.text);
        }
    }
}

void appendHtmlBitfieldSemantic(
    std::ostringstream& body,
    const nlohmann::ordered_json& semantic,
    const std::string& tagHex,
    const std::string& hexDisplay,
    const std::string& rawHex,
    emvparse::SemanticSummary mode
) {
    const auto summaryLines = buildSemanticSummaryLines(semantic, emvparse::SemanticSummary::Active);
    const auto fullLines = buildSemanticSummaryLines(semantic, emvparse::SemanticSummary::Full);

    if (mode == emvparse::SemanticSummary::Full) {
        appendHtmlInlineSemanticLines(body, fullLines, hexDisplay, rawHex, true);
        return;
    }

    size_t summaryStart = 0;
    if (!summaryLines.empty() &&
        !semanticLineRedundantWithHex(summaryLines[0].text, hexDisplay, rawHex)) {
        body << "<br><strong>" << htmlEscape(summaryLines[0].text) << "</strong>";
        summaryStart = 1;
    }

    if (summaryLines.empty() && fullLines.empty()) {
        return;
    }

    const std::string detailId = "sem-" + tagHex + "-" + std::to_string(++g_htmlSemanticCounter);
    body << "<br><span class=\"semantic-toggle\" onclick=\"toggleSemantic('" << detailId
         << "')\" title=\"Click to expand/collapse details\"><strong>Details</strong></span>";
    body << "<div id=\"summary-" << detailId << "\" class=\"semantic-summary\">";
    for (size_t i = summaryStart; i < summaryLines.size(); ++i) {
        if (semanticLineRedundantWithHex(summaryLines[i].text, hexDisplay, rawHex)) {
            continue;
        }
        body << htmlEscape(summaryLines[i].text) << "<br>";
    }
    body << "</div>";
    body << "<div id=\"full-" << detailId << "\" class=\"semantic-full\" style=\"display:none\">";
    for (const auto& line : fullLines) {
        if (line.inactive) {
            body << "<span class=\"inactive-bit\">" << htmlEscape(line.text) << "</span><br>";
        } else {
            body << htmlEscape(line.text) << "<br>";
        }
    }
    body << "</div>";
}

void appendHtmlRows(
    std::ostringstream& body,
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth,
    int maxDepth,
    const emvparse::FormatOptions& options
) {
    const std::string tagHex = tlv->tagHex();
    std::shared_ptr<TagInfo> tagInfo = registry ? registry->getTagInfo(tagHex) : nullptr;
    const bool composite = !tlv->getChildren().empty();

    body << "<tr>";
    if (maxDepth > 0) {
        for (int i = 0; i < depth; ++i) {
            body << "<td class=\"tlv-gutter\"></td>";
        }
    }
    body << "<td class=\"tlv-tag\">" << htmlEscape(tagHex) << "</td>";
    for (int i = 0; i < maxDepth - depth; ++i) {
        body << "<td class=\"tlv-indent-pad\"></td>";
    }
    body << "<td class=\"tlv-name\">" << htmlEscape(tagInfo ? tagInfo->name : "") << "</td>";
    body << "<td class=\"tlv-len\">" << tlv->getValue().size() << "</td>";
    body << "<td class=\"value-cell\">";

    if (!composite) {
        const std::string hexDisplay = spacedHexUpper(tlv->valueHex());
        body << htmlEscape(hexDisplay);

        if (tagInfo) {
            const auto semantic = semanticForTlv(tlv, registry, tagInfo);
            const auto mode = effectiveSemanticSummaryMode(tagInfo->type, options.semantic);
            if (tagInfo->type == "BITFIELD") {
                appendHtmlBitfieldSemantic(body, semantic, tagHex, hexDisplay, tlv->valueHex(), mode);
            } else {
                const auto lines = buildSemanticSummaryLines(semantic, mode);
                appendHtmlInlineSemanticLines(body, lines, hexDisplay, tlv->valueHex(), false);
            }
        }
    }

    body << "</td></tr>\n";

    for (const auto& child : tlv->getChildren()) {
        appendHtmlRows(body, child, registry, depth + 1, maxDepth, options);
    }
}

} // namespace

std::vector<TableTextRow> TLVFormatter::tableRowsFromRoots(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::vector<TableTextRow> rows;
    for (const auto& root : roots) {
        collectTableRows(root, registry, 0, options, rows);
    }
    return rows;
}

std::string TLVFormatter::renderTextTable(const std::vector<TableTextRow>& rows) {
    std::ostringstream out;
    renderFixedWidthTable(rows, out);
    return out.str();
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

std::string TLVFormatter::toTable(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    return renderTextTable(tableRowsFromRoots(roots, registry, options));
}

const char* TLVFormatter::htmlStyles() {
    return ".tlv-wrap{margin-top:8px;overflow-x:auto;}\n"
           ".tlv-table{border-collapse:collapse;width:max-content;max-width:100%;"
           "font-size:14px;}\n"
           ".tlv-table col.tlv-col-indent{width:14px;}\n"
           ".tlv-table th,.tlv-table td{border:1px solid #333;padding:4px 8px;vertical-align:top;}\n"
           ".tlv-table th{background:#eee;text-align:center;}\n"
           ".tlv-table td.tlv-gutter{width:14px;min-width:14px;max-width:14px;padding:0;"
           "background:#d8f0dc;border-color:#aaa;}\n"
           ".tlv-table td.tlv-indent-pad{width:14px;min-width:14px;max-width:14px;padding:0;"
           "border-color:#ddd;background:#fafafa;}\n"
           ".tlv-table td.tlv-tag{font-family:Consolas,monospace;font-weight:600;white-space:nowrap;"
           "background:#f5fbf6;}\n"
           ".tlv-table th.tlv-len,.tlv-table td.tlv-len{text-align:right;white-space:nowrap;"
           "width:1%;}\n"
           ".tlv-table th.tlv-name,.tlv-table td.tlv-name{white-space:normal;max-width:36em;}\n"
           ".tlv-table td.value-cell{font-family:Consolas,monospace;word-break:break-word;"
           "overflow-wrap:anywhere;min-width:12em;}\n"
           ".semantic-toggle{cursor:pointer;color:#0645ad;}\n"
           ".semantic-summary{margin-top:4px;font-size:0.95em;}\n"
           ".semantic-full{margin-top:4px;font-size:0.95em;}\n"
           ".inactive-bit{color:#808080;}\n";
}

const char* TLVFormatter::htmlToggleScript() {
    return "function toggleSemantic(id){\n"
           "  var full=document.getElementById('full-'+id);\n"
           "  if(!full){return;}\n"
           "  full.style.display=(full.style.display==='none')?'block':'none';\n"
           "}\n";
}

std::string TLVFormatter::toHtmlFragment(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    g_htmlSemanticCounter = 0;
    const int maxDepth = computeMaxTlvDepth(roots);
    std::ostringstream body;
    for (const auto& root : roots) {
        appendHtmlRows(body, root, registry, 0, maxDepth, options);
    }

    const int indentCols = maxDepth + 1;
    std::ostringstream frag;
    frag << "<div class=\"tlv-wrap\"><table class=\"tlv-table\">\n<colgroup>";
    for (int i = 0; i < indentCols; ++i) {
        frag << "<col class=\"tlv-col-indent\">";
    }
    frag << "<col class=\"tlv-col-name\"><col class=\"tlv-col-len\"><col class=\"tlv-col-value\"></colgroup>\n"
         << "<thead><tr><th colspan=\"" << indentCols << "\"></th>"
         << "<th class=\"tlv-name\">Name</th><th class=\"tlv-len\">Length</th>"
         << "<th class=\"tlv-value\">Value</th></tr></thead>\n<tbody>\n"
         << body.str()
         << "</tbody></table></div>\n";
    return frag.str();
}

std::string TLVFormatter::toHtml(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    const std::string fragment = toHtmlFragment(roots, registry, options);
    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<title>EMV TLV Parse</title>\n<style>\n"
        << "body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}\n"
        << TLVFormatter::htmlStyles()
        << "</style>\n<script>\n"
        << TLVFormatter::htmlToggleScript()
        << "</script>\n</head>\n<body>\n"
        << "<h1>EMV TLV</h1>\n"
        << fragment
        << "</body>\n</html>\n";
    return doc.str();
}
