#ifndef EMVPARSE_TAG_REGISTRY_OPS_H
#define EMVPARSE_TAG_REGISTRY_OPS_H

#include "emvparse/emvparse_global.h"
#include <cstddef>
#include <string>
#include <vector>

struct RegistryIssue {
    std::string level;
    std::string message;
    std::string tag;
};

struct SchemeValidationResult {
    std::string scheme;
    bool loadOk = false;
    std::string loadError;
    std::size_t tagCount = 0;
    std::vector<RegistryIssue> issues;
};

struct RegistryValidationResult {
    std::vector<SchemeValidationResult> schemes;

    bool allOk() const {
        for (const auto& s : schemes) {
            if (!s.loadOk) {
                return false;
            }
            for (const auto& issue : s.issues) {
                if (issue.level == "error") {
                    return false;
                }
            }
        }
        return !schemes.empty();
    }
};

EMVPARSE_EXPORT RegistryValidationResult validateAllSchemes(const std::string& manifestPath);

EMVPARSE_EXPORT bool mergeSchemeToFile(
    const std::string& manifestPath,
    const std::string& scheme,
    const std::string& outPath,
    std::string& errorOut);

#endif // EMVPARSE_TAG_REGISTRY_OPS_H
