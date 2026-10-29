#include "cli_options.h"
#include "commands_script.h"
#include "hex_util.h"
#include "manifest_resolve.h"
#include <emvparse/tlv_parser.h>
#include <emvparse/transaction_script.h>
#include <emvparse/output_options.h>
#include <cstring>
#include <iostream>

namespace {

void printScriptUsage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  emvparse script --file <path.json> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --format json|table|html  Output format (default: json)\n"
        << "  --output, -o <path>       Write HTML to file (default: <script>.html; - for stdout)\n"
        << "  --semantic active|full    Semantic detail for table only (default: active)\n"
        << "  --scheme <EMV|Visa|UPI>   Registry scheme (default: EMV)\n"
        << "  --registry <path>         manifest.json or registry directory\n"
        << "  --manifest <path>         Alias for --registry\n";
}

struct CommonOpts {
    std::string scheme = "EMV";
    std::string manifestOverride;
};

struct ScriptFormatOpts {
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

bool parseScriptFormatOptions(int& index, int argc, char* argv[], ScriptFormatOpts& fmt) {
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

int runScriptCommand(int argc, char* argv[]) {
    for (int j = 2; j < argc; ++j) {
        if (isHelpOption(argv[j])) {
            printScriptUsage(std::cout);
            return 0;
        }
    }

    if (argc < 3) {
        printScriptUsage(std::cerr);
        return 1;
    }

    std::string scriptPath;
    int i = 2;
    if (std::strcmp(argv[i], "--file") != 0 || i + 1 >= argc) {
        printScriptUsage(std::cerr);
        return 1;
    }
    scriptPath = argv[i + 1];
    i += 2;

    ScriptFormatOpts fmt;
    if (!parseScriptFormatOptions(i, argc, argv, fmt)) {
        return 1;
    }

    CommonOpts opts;
    parseCommonOptions(i, argc, argv, opts);
    if (i < argc) {
        printScriptUsage(std::cerr);
        return 1;
    }

    if (fmt.semanticExplicit && fmt.format != emvparse::OutputFormat::Table) {
        std::cerr << "note: --semantic applies to --format table only; ignored for this output format\n";
    }
    if (fmt.outputExplicit && fmt.format != emvparse::OutputFormat::Html) {
        std::cerr << "error: --output applies to --format html only\n";
        return 1;
    }

    TransactionScript script;
    std::string loadError;
    if (!TransactionScriptParser::loadFromFile(scriptPath, script, loadError)) {
        std::cerr << "error: " << loadError << "\n";
        return 2;
    }

    const std::string scheme = !script.scheme.empty() ? script.scheme : opts.scheme;
    const std::string manifest = resolveManifestPath(opts.manifestOverride);
    if (manifest.empty()) {
        std::cerr << "error: manifest not found (set --registry or EMVPARSE_REGISTRY)\n";
        return 2;
    }

    TLVParser parser(manifest, scheme);
    TransactionScriptParseResult parsed = TransactionScriptParser::parse(parser, script);
    if (!parsed.error.empty()) {
        std::cerr << "error: " << parsed.error << "\n";
        return 3;
    }

    emvparse::FormatOptions formatOptions;
    formatOptions.semantic = fmt.semantic;

    std::string output;
    TagRegistry* tagRegistry = parser.tagRegistry();
    switch (fmt.format) {
    case emvparse::OutputFormat::Table:
        output = TransactionScriptFormatter::formatTable(parsed, tagRegistry, formatOptions);
        break;
    case emvparse::OutputFormat::Html:
        output = TransactionScriptFormatter::formatHtml(parsed, tagRegistry, formatOptions);
        break;
    case emvparse::OutputFormat::Json:
    default:
        output = TransactionScriptFormatter::formatJson(parsed, tagRegistry);
        break;
    }

    if (fmt.format == emvparse::OutputFormat::Html) {
        const std::string path =
            fmt.outputExplicit ? fmt.outputPath : defaultHtmlReportPath(scriptPath);
        if (path == "-") {
            std::cout << output;
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
