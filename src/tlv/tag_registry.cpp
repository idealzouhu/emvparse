#include "tag_registry.h"
#include "registry_io.h"
#include "iso_numeric_lookup.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace {

std::string normalizeTagHex(std::string tag) {
    std::string out;
    out.reserve(tag.size());
    for (unsigned char c : tag) {
        if (std::isspace(c)) {
            continue;
        }
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

bool isValidTagHex(const std::string& tag) {
    if (tag.empty() || tag.size() > 8) {
        return false;
    }
    for (unsigned char c : tag) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    return true;
}

std::string normalizeTagType(std::string type) {
    if (type == "N/A" || type == "CERTIFICATE") {
        return "BINARY";
    }
    return type;
}

void normalizeBitFieldRule(BitFieldRule& rule) {
    if (rule.type == "BOOLEAN") {
        rule.type = "ENUM";
        if (rule.enumMap.empty()) {
            rule.enumMap["0"] = "false";
            rule.enumMap["1"] = "true";
        }
    }
}

void applyStringField(json const& obj, const char* key, std::string& dest) {
    if (obj.contains(key) && obj[key].is_string()) {
        dest = obj[key].get<std::string>();
    }
}

std::shared_ptr<TagInfo> buildTagInfoFromObject(const json& tagJson) {
    auto info = std::make_shared<TagInfo>();

    if (!tagJson.contains("tag") || !tagJson["tag"].is_string()) {
        return nullptr;
    }
    info->tag = normalizeTagHex(tagJson["tag"].get<std::string>());
    if (!isValidTagHex(info->tag)) {
        return nullptr;
    }

    applyStringField(tagJson, "name", info->name);
    applyStringField(tagJson, "description", info->description);
    applyStringField(tagJson, "format", info->format);
    applyStringField(tagJson, "length", info->length);
    applyStringField(tagJson, "source", info->source);
    applyStringField(tagJson, "note", info->note);
    applyStringField(tagJson, "template", info->template_);

    if (tagJson.contains("scheme") && tagJson["scheme"].is_string()) {
        info->kernel = tagJson["scheme"].get<std::string>();
    }
    applyStringField(tagJson, "kernel", info->kernel);

    if (tagJson.contains("type") && tagJson["type"].is_string()) {
        info->type = normalizeTagType(tagJson["type"].get<std::string>());
    }

    if (tagJson.contains("enumMap") && tagJson["enumMap"].is_object()) {
        for (auto it = tagJson["enumMap"].begin(); it != tagJson["enumMap"].end(); ++it) {
            if (it.value().is_string()) {
                info->enumMap[it.key()] = it.value().get<std::string>();
            }
        }
    }
    applyStringField(tagJson, "enumRef", info->enumRef);

    if (info->type == "BITFIELD" && tagJson.contains("bitFields") && tagJson["bitFields"].is_array()) {
        for (const auto& fieldJson : tagJson["bitFields"]) {
            BitFieldRule rule;
            if (fieldJson.contains("position") && fieldJson["position"].is_number_integer()) {
                rule.position = fieldJson["position"].get<int>();
            }
            if (fieldJson.contains("length") && fieldJson["length"].is_number_integer()) {
                rule.length = fieldJson["length"].get<int>();
            }
            applyStringField(fieldJson, "type", rule.type);
            applyStringField(fieldJson, "name", rule.name);
            if (rule.type == "ENUM" && fieldJson.contains("enumMap") && fieldJson["enumMap"].is_object()) {
                for (auto it = fieldJson["enumMap"].begin(); it != fieldJson["enumMap"].end(); ++it) {
                    if (it.value().is_string()) {
                        rule.enumMap[it.key()] = it.value().get<std::string>();
                    }
                }
            }
            normalizeBitFieldRule(rule);
            info->bitFields.push_back(rule);
        }
    }

    return info;
}

json tagBodyForRuntime(const json& tagJson, Logger& log) {
    if (tagJson.contains("variants") && tagJson["variants"].is_array() && !tagJson["variants"].empty()) {
        const auto& variants = tagJson["variants"];
        if (variants.size() > 1) {
            std::string tagHex = tagJson.contains("tag") ? tagJson["tag"].get<std::string>() : "?";
            log.warning(
                "Tag " + tagHex + " has " + std::to_string(variants.size()) +
                " variants; runtime flat map uses the first variant only");
        }
        json merged = variants[0];
        merged["tag"] = tagJson["tag"];
        return merged;
    }
    return tagJson;
}

} // namespace

void TagRegistry::setLogger(Logger* logger) {
    logger_ = logger;
}

Logger& TagRegistry::logSink() const {
    return logger_ ? *logger_ : Logger::defaultSink();
}

bool TagRegistry::loadManifest(const std::string& manifestPath, const std::string& scheme) {
    // Merge order: base file, then each overlay path listed under manifest.overlays[scheme].
    tagMap_.clear();
    tagSourceFile_.clear();

    json manifest;
    Logger& log = logSink();
    if (!readRegistryJsonFile(manifestPath, manifest, log)) {
        return false;
    }
    if (!manifest.is_object() || !manifest.contains("base") || !manifest["base"].is_string()) {
        log.error("Invalid manifest: missing string field 'base'");
        return false;
    }
    if (!manifest.contains("overlays") || !manifest["overlays"].is_object()) {
        log.error("Invalid manifest: missing object field 'overlays'");
        return false;
    }

    const std::string dir = registryDirname(manifestPath);
    const std::string baseFile = joinRegistryPath(dir, manifest["base"].get<std::string>());

    MergeLayer baseLayer;
    baseLayer.filePath = baseFile;
    baseLayer.schemeLabel = "base";
    if (!loadRegistryFile(baseFile, baseLayer)) {
        return false;
    }

    const json& overlays = manifest["overlays"];
    if (!overlays.contains(scheme)) {
        log.warning("Manifest has no overlays entry for scheme: " + scheme);
    } else if (overlays[scheme].is_array()) {
        for (const auto& entry : overlays[scheme]) {
            if (!entry.is_string()) {
                continue;
            }
            const std::string overlayPath = joinRegistryPath(dir, entry.get<std::string>());
            MergeLayer layer;
            layer.filePath = overlayPath;
            layer.schemeLabel = scheme;
            if (!loadRegistryFile(overlayPath, layer)) {
                log.error("Failed to load overlay: " + overlayPath);
                return false;
            }
        }
    }

    if (tagMap_.empty()) {
        log.warning("No tags loaded for scheme: " + scheme);
        return false;
    }

    emvparse::loadIsoNumericLookupsFromRegistryDir(dir);

    log.info(
        "Tag registry loaded for scheme " + scheme + ": " + std::to_string(tagMap_.size()) + " tags");
    return true;
}

bool TagRegistry::loadRegistryFile(const std::string& filePath, const MergeLayer& layer) {
    Logger& log = logSink();
    json root;
    if (!readRegistryJsonFile(filePath, root, log)) {
        return false;
    }

    if (!root.is_object() || !root.contains("tags") || !root["tags"].is_array()) {
        log.error("Invalid registry format (expected object with tags array): " + filePath);
        return false;
    }

    std::string fileScheme = layer.schemeLabel;
    if (root.contains("metadata") && root["metadata"].is_object()) {
        const json& md = root["metadata"];
        if (md.contains("scheme") && md["scheme"].is_string()) {
            fileScheme = md["scheme"].get<std::string>();
        }
    }

    MergeLayer effectiveLayer = layer;
    effectiveLayer.schemeLabel = fileScheme;

    int loadedCount = 0;
    int skippedCount = 0;

    for (const auto& tagJson : root["tags"]) {
        if (!tagJson.is_object()) {
            ++skippedCount;
            continue;
        }
        const json runtimeJson = tagBodyForRuntime(tagJson, log);
        auto info = buildTagInfoFromObject(runtimeJson);
        if (!info) {
            ++skippedCount;
            log.warning("Skipping invalid tag entry in " + filePath);
            continue;
        }
        if (info->kernel.empty()) {
            info->kernel = effectiveLayer.schemeLabel;
        }

        const auto prev = tagSourceFile_.find(info->tag);
        if (prev != tagSourceFile_.end()) {
            log.info(
                "Tag " + info->tag + " overridden: " + prev->second + " -> " + filePath);
        }

        tagMap_[info->tag] = info;
        tagSourceFile_[info->tag] = filePath;
        ++loadedCount;
    }

    log.info(
        "Loaded " + std::to_string(loadedCount) + " tags from " + filePath +
        (skippedCount > 0 ? " (skipped " + std::to_string(skippedCount) + ")" : ""));
    return loadedCount > 0 || layer.schemeLabel != "base";
}

std::shared_ptr<TagInfo> TagRegistry::getTagInfo(const std::string& tagHex) const {
    const std::string key = normalizeTagHex(tagHex);
    const auto it = tagMap_.find(key);
    if (it != tagMap_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<TagInfo>> TagRegistry::allTags() const {
    std::vector<std::shared_ptr<TagInfo>> out;
    out.reserve(tagMap_.size());
    for (const auto& pair : tagMap_) {
        out.push_back(pair.second);
    }
    std::sort(out.begin(), out.end(), [](const std::shared_ptr<TagInfo>& a, const std::shared_ptr<TagInfo>& b) {
        return a->tag < b->tag;
    });
    return out;
}
