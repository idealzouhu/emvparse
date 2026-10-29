#include "cli_options.h"
#include "commands_registry.h"
#include "manifest_resolve.h"
#include "tag_registry_ops.h"
#include <cstring>
#include <iostream>

namespace {

void printRegistryUsage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  emvparse registry validate [options]\n"
        << "  emvparse registry merge --scheme <EMV|Visa|UPI> --output <file> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --registry <path>         manifest.json or registry directory\n"
        << "  --manifest <path>         Alias for --registry\n"
        << "  --scheme <EMV|Visa|UPI>   Required for merge\n"
        << "  --output <file>, -o       Required for merge (merged JSON path)\n";
}

std::string parseRegistryPathOverride(int argc, char* argv[], int startIndex) {
    std::string manifestOverride;
    for (int i = startIndex; i < argc; ++i) {
        if (isRegistryPathOption(argv[i]) && i + 1 < argc) {
            manifestOverride = argv[++i];
        }
    }
    return manifestOverride;
}

} // namespace

int runRegistryCommand(int argc, char* argv[]) {
    for (int j = 2; j < argc; ++j) {
        if (isHelpOption(argv[j])) {
            printRegistryUsage(std::cout);
            return 0;
        }
    }

    if (argc < 3) {
        printRegistryUsage(std::cerr);
        return 1;
    }

    const char* sub = argv[2];
    if (isHelpOption(sub)) {
        printRegistryUsage(std::cout);
        return 0;
    }

    const std::string manifestOverride = parseRegistryPathOverride(argc, argv, 3);

    const std::string manifest = resolveManifestPath(manifestOverride);
    if (manifest.empty()) {
        std::cerr << "error: manifest not found (set --registry or EMVPARSE_REGISTRY)\n";
        return 2;
    }

    if (std::strcmp(sub, "validate") == 0) {
        const RegistryValidationResult result = validateAllSchemes(manifest);
        bool ok = true;
        for (const auto& scheme : result.schemes) {
            std::cout << "Scheme " << scheme.scheme << ": ";
            if (!scheme.loadOk) {
                std::cout << "load failed";
                if (!scheme.loadError.empty()) {
                    std::cout << " (" << scheme.loadError << ")";
                }
                std::cout << "\n";
                ok = false;
                continue;
            }
            std::cout << scheme.tagCount << " tags\n";
            for (const auto& issue : scheme.issues) {
                std::cout << "  [" << issue.level << "]";
                if (!issue.tag.empty()) {
                    std::cout << " tag " << issue.tag;
                }
                std::cout << " " << issue.message << "\n";
                if (issue.level == "error") {
                    ok = false;
                }
            }
        }
        return ok ? 0 : 1;
    }

    if (std::strcmp(sub, "merge") == 0) {
        std::string scheme;
        std::string outPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--scheme") == 0 && i + 1 < argc) {
                scheme = argv[++i];
            } else if (isOutputPathOption(argv[i]) && i + 1 < argc) {
                outPath = argv[++i];
            }
        }
        if (scheme.empty() || outPath.empty()) {
            printRegistryUsage(std::cerr);
            return 1;
        }
        std::string err;
        if (!mergeSchemeToFile(manifest, scheme, outPath, err)) {
            std::cerr << err << "\n";
            return 2;
        }
        std::cout << "Wrote merged registry to " << outPath << "\n";
        return 0;
    }

    printRegistryUsage(std::cerr);
    return 1;
}
