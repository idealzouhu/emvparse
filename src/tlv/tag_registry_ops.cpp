#include "tag_registry_ops.h"
#include "tag_registry.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {

bool validateLoadedRegistry(const TagRegistry& registry, SchemeValidationResult& out) {
    bool ok = true;
    if (registry.tagCount() == 0) {
        RegistryIssue issue;
        issue.level = "error";
        issue.message = "no tags loaded";
        out.issues.push_back(issue);
        return false;
    }
    for (const auto& info : registry.allTags()) {
        if (info->tag.empty()) {
            RegistryIssue issue;
            issue.level = "error";
            issue.message = "empty tag hex";
            out.issues.push_back(issue);
            ok = false;
            continue;
        }
        if (info->type == "ENUM" && info->enumMap.empty()) {
            RegistryIssue issue;
            issue.level = "error";
            issue.tag = info->tag;
            issue.message = "ENUM missing enumMap";
            out.issues.push_back(issue);
            ok = false;
        }
        if (info->type == "BITFIELD" && info->bitFields.empty()) {
            RegistryIssue issue;
            issue.level = "warning";
            issue.tag = info->tag;
            issue.message = "BITFIELD has no bitFields (semantic pending)";
            out.issues.push_back(issue);
        }
        for (const auto& rule : info->bitFields) {
            if (rule.type == "ENUM" && rule.enumMap.empty()) {
                RegistryIssue issue;
                issue.level = "error";
                issue.tag = info->tag;
                issue.message = "bitfield ENUM missing enumMap";
                out.issues.push_back(issue);
                ok = false;
            }
        }
    }
    return ok;
}

json tagInfoToJson(const TagInfo& info) {
    json j;
    j["tag"] = info.tag;
    if (!info.name.empty()) j["name"] = info.name;
    if (!info.description.empty()) j["description"] = info.description;
    if (!info.type.empty()) j["type"] = info.type;
    if (!info.format.empty()) j["format"] = info.format;
    if (!info.length.empty()) j["length"] = info.length;
    if (!info.source.empty()) j["source"] = info.source;
    if (!info.kernel.empty()) j["scheme"] = info.kernel;
    if (!info.template_.empty()) j["template"] = info.template_;
    if (!info.note.empty()) j["note"] = info.note;
    if (!info.enumMap.empty()) {
        j["enumMap"] = info.enumMap;
    }
    if (!info.bitFields.empty()) {
        json arr = json::array();
        for (const auto& rule : info.bitFields) {
            json bf;
            bf["position"] = rule.position;
            bf["length"] = rule.length;
            bf["type"] = rule.type;
            if (!rule.name.empty()) bf["name"] = rule.name;
            if (!rule.enumMap.empty()) bf["enumMap"] = rule.enumMap;
            arr.push_back(bf);
        }
        j["bitFields"] = arr;
    }
    return j;
}

} // namespace

RegistryValidationResult validateAllSchemes(const std::string& manifestPath) {
    RegistryValidationResult result;
    std::ifstream in(manifestPath, std::ios::binary);
    if (!in.is_open()) {
        SchemeValidationResult schemeResult;
        schemeResult.scheme = "(manifest)";
        schemeResult.loadOk = false;
        schemeResult.loadError = "failed to open manifest";
        result.schemes.push_back(schemeResult);
        return result;
    }
    json manifest = json::parse(in, nullptr, false);
    if (manifest.is_discarded() || !manifest.contains("overlays") || !manifest["overlays"].is_object()) {
        SchemeValidationResult schemeResult;
        schemeResult.scheme = "(manifest)";
        schemeResult.loadOk = false;
        schemeResult.loadError = "invalid manifest overlays";
        result.schemes.push_back(schemeResult);
        return result;
    }
    for (auto it = manifest["overlays"].begin(); it != manifest["overlays"].end(); ++it) {
        const std::string scheme = it.key();
        SchemeValidationResult schemeResult;
        schemeResult.scheme = scheme;
        TagRegistry reg;
        if (!reg.loadManifest(manifestPath, scheme)) {
            schemeResult.loadOk = false;
            schemeResult.loadError = "failed to load manifest for scheme";
        } else {
            schemeResult.loadOk = true;
            schemeResult.tagCount = reg.tagCount();
            validateLoadedRegistry(reg, schemeResult);
        }
        result.schemes.push_back(schemeResult);
    }
    return result;
}

bool mergeSchemeToFile(
    const std::string& manifestPath,
    const std::string& scheme,
    const std::string& outPath,
    std::string& errorOut) {
    TagRegistry reg;
    if (!reg.loadManifest(manifestPath, scheme)) {
        errorOut = "failed to load manifest for scheme " + scheme;
        return false;
    }

    json root;
    root["metadata"]["scheme"] = scheme;
    root["metadata"]["mergedFrom"] = manifestPath;
    json tags = json::array();
    for (const auto& info : reg.allTags()) {
        tags.push_back(tagInfoToJson(*info));
    }
    root["tags"] = tags;

    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
        errorOut = "failed to write " + outPath;
        return false;
    }
    out << root.dump(2) << "\n";
    if (!out.good()) {
        errorOut = "write failed for " + outPath;
        return false;
    }
    return true;
}
