#include "cli_options.h"
#include "commands_parse.h"
#include "hex_util.h"
#include "manifest_resolve.h"
#include <emvparse/tlv_parser.h>
#include <emvparse/output_options.h>
#include <cstring>
#include <iostream>

namespace {

void printParseUsage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  emvparse parse <hex> [options]\n"
        << "  emvparse parse --file <path> [options]\n"
        << "  emvparse parse - [options]          Read hex from stdin\n"
        << "\n"
        << "Options:\n"
        << "  --format json|table|html  Output format (default: json)\n"
        << "  --output, -o <path>       Write HTML to file (default: report.html, or\n"
        << "                            <hex-file>.html with --file; use - for stdout)\n"
        << "  --semantic active|full    Semantic detail for table only (default: active)\n"
        << "  --scheme <EMV|Visa|UPI>   Registry scheme (default: EMV)\n"
        << "  --registry <path>         manifest.json or registry directory\n"
        << "  --manifest <path>         Alias for --registry\n";
}

void printTagInfoUsage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  emvparse tag-info <tag-hex> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --scheme <EMV|Visa|UPI>   Registry scheme (default: EMV)\n"
        << "  --registry <path>         manifest.json or registry directory\n"
        << "  --manifest <path>         Alias for --registry\n";
}

struct CommonOpts {
    std::string scheme = "EMV";
    std::string manifestOverride;
};

struct ParseFormatOpts {
    emvparse::OutputFormat format = emvparse::OutputFormat::Json;
    emvparse::SemanticSummary semantic = emvparse::SemanticSummary::Active;
    bool semanticExplicit = false;
    std::string outputPath;
    bool outputExplicit = false;
};

bool parseOutputFormat(const char* value, emvparse::OutputFormat& out) {
    if (std::strcmp(value, "json") == 0) {
        out = emvparse::OutputFormat::Json;
        return true;
    }
    if (std::strcmp(value, "table") == 0) {
        out = emvparse::OutputFormat::Table;
        return true;
    }
    if (std::strcmp(value, "html") == 0) {
        out = emvparse::OutputFormat::Html;
        return true;
    }
    return false;
}

bool parseSemanticMode(const char* value, emvparse::SemanticSummary& out) {
    if (std::strcmp(value, "active") == 0) {
        out = emvparse::SemanticSummary::Active;
        return true;
    }
    if (std::strcmp(value, "full") == 0) {
        out = emvparse::SemanticSummary::Full;
        return true;
    }
    return false;
}

bool parseCommonOptions(int& index, int argc, char* argv[], CommonOpts& opts) {
    for (; index < argc; ++index) {
        if (std::strcmp(argv[index], "--scheme") == 0 && index + 1 < argc) {
            opts.scheme = argv[++index];
        } else if (isRegistryPathOption(argv[index]) && index + 1 < argc) {
            opts.manifestOverride = argv[++index];
        } else {
            break;
        }
    }
    return true;
}

bool parseParseFormatOptions(int& index, int argc, char* argv[], ParseFormatOpts& fmt) {
    for (; index < argc; ++index) {
        if (std::strcmp(argv[index], "--format") == 0 && index + 1 < argc) {
            if (!parseOutputFormat(argv[index + 1], fmt.format)) {
                std::cerr << "error: invalid --format (use json, table, or html)\n";
                return false;
            }
            ++index;
        } else if (std::strcmp(argv[index], "--semantic") == 0 && index + 1 < argc) {
            if (!parseSemanticMode(argv[index + 1], fmt.semantic)) {
                std::cerr << "error: invalid --semantic (use active or full)\n";
                return false;
            }
            fmt.semanticExplicit = true;
            ++index;
        } else if ((std::strcmp(argv[index], "--output") == 0 || std::strcmp(argv[index], "-o") == 0) &&
                   index + 1 < argc) {
            fmt.outputPath = argv[index + 1];
            fmt.outputExplicit = true;
            ++index;
        } else {
            break;
        }
    }
    return true;
}

} // namespace

int runParseCommand(int argc, char* argv[]) {
    for (int j = 2; j < argc; ++j) {
        if (isHelpOption(argv[j])) {
            printParseUsage(std::cout);
            return 0;
        }
    }

    if (argc < 3) {
        printParseUsage(std::cerr);
        return 1;
    }

    CommonOpts opts;
    std::string hexInput;
    std::string hexSourcePath;
    bool fromFile = false;
    bool fromStdin = false;
    int i = 2;
    if (std::strcmp(argv[i], "--file") == 0) {
        if (i + 1 >= argc) {
            printParseUsage(std::cerr);
            return 1;
        }
        fromFile = true;
        ++i;
        hexSourcePath = argv[i];
        std::string err;
        if (!readHexFromFile(hexSourcePath, hexInput, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        ++i;
    } else if (std::strcmp(argv[i], "-") == 0) {
        fromStdin = true;
        ++i;
        std::string err;
        if (!readHexFromStdin(hexInput, err)) {
            std::cerr << err << "\n";
            return 1;
        }
    } else {
        hexInput = normalizeHexInput(argv[i]);
        ++i;
    }

    ParseFormatOpts fmt;
    if (!parseParseFormatOptions(i, argc, argv, fmt)) {
        return 1;
    }
    parseCommonOptions(i, argc, argv, opts);
    if (i < argc) {
        printParseUsage(std::cerr);
        return 1;
    }

    if (fmt.semanticExplicit && fmt.format != emvparse::OutputFormat::Table) {
        std::cerr << "note: --semantic applies to --format table only; ignored for this output format\n";
    }
    if (fmt.outputExplicit && fmt.format != emvparse::OutputFormat::Html) {
        std::cerr << "error: --output applies to --format html only\n";
        return 1;
    }

    if (!fromFile && !fromStdin && hexInput.empty()) {
        printParseUsage(std::cerr);
        return 1;
    }

    const std::string manifest = resolveManifestPath(opts.manifestOverride);
    if (manifest.empty()) {
        std::cerr << "error: manifest not found (set --registry or EMVPARSE_REGISTRY)\n";
        return 2;
    }

    TLVParser parser(manifest, opts.scheme);
    emvparse::ParseOutputOptions outputOpts;
    outputOpts.format = fmt.format;
    outputOpts.formatOptions.semantic = fmt.semantic;

    const std::string output = parser.parseHexWithOptions(hexInput, outputOpts);
    if (fmt.format == emvparse::OutputFormat::Json) {
        if (output == "[]" && !hexInput.empty()) {
            std::cerr << "parse failed: invalid hex input\n";
            return 3;
        }
    } else if (output.empty() && !hexInput.empty()) {
        std::cerr << "parse failed: invalid hex input\n";
        return 3;
    }
    if (fmt.format == emvparse::OutputFormat::Html) {
        const std::string path =
            fmt.outputExplicit ? fmt.outputPath : defaultHtmlReportPath(hexSourcePath);
        if (path == "-") {
            std::cout << output;
            if (!output.empty() && output.back() != '\n') {
                std::cout << "\n";
            }
        } else {
            std::string err;
            if (!writeTextToFile(path, output, err)) {
                std::cerr << err << "\n";
                return 4;
            }
            std::cerr << "wrote " << path << "\n";
        }
    } else {
        std::cout << output;
        if (!output.empty() && output.back() != '\n') {
            std::cout << "\n";
        }
    }
    return 0;
}

int runTagInfoCommand(int argc, char* argv[]) {
    for (int j = 2; j < argc; ++j) {
        if (isHelpOption(argv[j])) {
            printTagInfoUsage(std::cout);
            return 0;
        }
    }

    if (argc < 3) {
        printTagInfoUsage(std::cerr);
        return 1;
    }

    CommonOpts opts;
    std::string tagHex = normalizeHexInput(argv[2]);
    int i = 3;
    parseCommonOptions(i, argc, argv, opts);
    if (i < argc || tagHex.empty()) {
        printTagInfoUsage(std::cerr);
        return 1;
    }

    const std::string manifest = resolveManifestPath(opts.manifestOverride);
    if (manifest.empty()) {
        std::cerr << "error: manifest not found (set --registry or EMVPARSE_REGISTRY)\n";
        return 2;
    }

    TLVParser parser(manifest, opts.scheme);
    const std::string info = parser.getTagInfo(tagHex);
    if (info.empty()) {
        std::cerr << "tag not found in registry: " << tagHex << "\n";
        return 2;
    }
    std::cout << info << "\n";
    return 0;
}
