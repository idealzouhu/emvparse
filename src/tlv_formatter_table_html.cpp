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

void appendTreeLines(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth,
    const emvparse::FormatOptions& options,
    std::ostringstream& out
) {
    const std::string tagHex = tlv->tagHex();
    std::shared_ptr<TagInfo> tagInfo = registry ? registry->getTagInfo(tagHex) : nullptr;
    const bool composite = !tlv->getChildren().empty();
    const std::string name = tagInfo ? tagInfo->name : "";
    out << indentSpaces(depth) << tagHex << " " << treeValueLengthHex(tlv->getValue().size());
    if (!name.empty()) {
        out << " -> " << name;
    }
    out << "\n";
    if (!composite && tagInfo) {
        const auto semantic = semanticForTlv(tlv, registry, tagInfo);
        const auto lines = buildSemanticSummaryLines(semantic, options.semantic);
        for (const auto& line : lines) {
            out << indentSpaces(depth + 1) << line.text << "\n";
        }
    }
    for (const auto& child : tlv->getChildren()) {
        appendTreeLines(child, registry, depth + 1, options, out);
    }
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
        const auto lines = buildSemanticSummaryLines(semantic, options.semantic);
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

void appendHtmlRows(
    std::ostringstream& body,
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    int depth
) {
    const std::string tagHex = tlv->tagHex();
    std::shared_ptr<TagInfo> tagInfo = registry ? registry->getTagInfo(tagHex) : nullptr;
    const bool composite = !tlv->getChildren().empty();

    body << "<tr>";
    body << "<td class=\"depth-" << depth << "\">" << htmlEscape(indentSpaces(depth) + tagHex) << "</td>";
    body << "<td>" << htmlEscape(tagInfo ? tagInfo->name : "") << "</td>";
    body << "<td>" << tlv->getValue().size() << "</td>";
    body << "<td class=\"value-cell\">";

    if (!composite) {
        const std::string hexDisplay = spacedHexUpper(tlv->valueHex());
        body << htmlEscape(hexDisplay);

        if (tagInfo) {
            const auto semantic = semanticForTlv(tlv, registry, tagInfo);
            const auto summaryLines = buildSemanticSummaryLines(semantic, emvparse::SemanticSummary::Active);
            const auto fullLines = buildSemanticSummaryLines(semantic, emvparse::SemanticSummary::Full);

            if (!summaryLines.empty() || !fullLines.empty()) {
                const std::string detailId = "sem-" + tagHex + "-" + std::to_string(++g_htmlSemanticCounter);
                body << "<br><span class=\"semantic-toggle\" onclick=\"toggleSemantic('" << detailId
                     << "')\" title=\"Click to expand/collapse details\"><strong>Details</strong></span>";
                body << "<div id=\"summary-" << detailId << "\" class=\"semantic-summary\">";
                for (const auto& line : summaryLines) {
                    body << htmlEscape(line.text) << "<br>";
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
        }
    }

    body << "</td></tr>\n";

    for (const auto& child : tlv->getChildren()) {
        appendHtmlRows(body, child, registry, depth + 1);
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
    for (const auto& root : roots) {
        appendTreeLines(root, registry, 0, options, out);
    }
    return out.str();
}

std::string TLVFormatter::toTable(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    return renderTextTable(tableRowsFromRoots(roots, registry, options));
}

std::string TLVFormatter::toHtml(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry
) {
    g_htmlSemanticCounter = 0;
    std::ostringstream body;
    for (const auto& root : roots) {
        appendHtmlRows(body, root, registry, 0);
    }

    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<title>EMV TLV Parse</title>\n<style>\n"
        << "body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}\n"
        << "table{border-collapse:collapse;width:100%;}\n"
        << "th,td{border:1px solid #ccc;padding:6px 8px;vertical-align:top;}\n"
        << "th{background:#eee;}\n"
        << ".value-cell{font-family:Consolas,monospace;}\n"
        << ".semantic-toggle{cursor:pointer;color:#0645ad;}\n"
        << ".semantic-summary{margin-top:4px;font-size:0.95em;}\n"
        << ".semantic-full{margin-top:4px;font-size:0.95em;}\n"
        << ".inactive-bit{color:#808080;}\n"
        << "td[class^=\"depth-\"]{background:#f9f9f9;}\n"
        << ".depth-1{background:#f3faf4;}\n"
        << ".depth-2{background:#eaf5ec;}\n"
        << "</style>\n<script>\n"
        << "function toggleSemantic(id){\n"
        << "  var full=document.getElementById('full-'+id);\n"
        << "  if(!full){return;}\n"
        << "  full.style.display=(full.style.display==='none')?'block':'none';\n"
        << "}\n"
        << "</script>\n</head>\n<body>\n"
        << "<h1>EMV TLV</h1>\n<table>\n"
        << "<thead><tr><th>TAG</th><th>Name</th><th>Length</th><th>Value</th></tr></thead>\n<tbody>\n"
        << body.str()
        << "</tbody></table>\n</body>\n</html>\n";
    return doc.str();
}
