#include "iso_numeric_lookup.h"
#include "registry_io.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_map>

namespace emvparse {

namespace {

std::mutex g_isoMutex;
bool g_isoLoaded = false;
std::unordered_map<std::string, std::string> g_iso3166;
std::unordered_map<std::string, std::string> g_iso4217;

void mergeStringMap(const nlohmann::json& obj, std::unordered_map<std::string, std::string>& dest) {
    if (!obj.is_object()) {
        return;
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().is_string()) {
            dest[it.key()] = it.value().get<std::string>();
        }
    }
}

} // namespace

void loadIsoNumericLookupsFromRegistryDir(const std::string& registryDir) {
    std::lock_guard<std::mutex> lock(g_isoMutex);
    if (g_isoLoaded) {
        return;
    }
    g_isoLoaded = true;

    const std::string path = joinRegistryPath(registryDir, "iso-lookups.json");
    nlohmann::json root;
    Logger& log = Logger::defaultSink();
    if (!readRegistryJsonFile(path, root, log)) {
        log.warning("ISO lookup table not loaded (optional): " + path);
        return;
    }
    if (root.contains("iso3166")) {
        mergeStringMap(root["iso3166"], g_iso3166);
    }
    if (root.contains("iso4217")) {
        mergeStringMap(root["iso4217"], g_iso4217);
    }
}

std::string isoNumericLookupKey(const std::string& bcdDigits) {
    std::string digits = bcdDigits;
    while (digits.size() > 1 && digits.front() == '0') {
        digits.erase(digits.begin());
    }
    if (digits.size() < 3) {
        digits.insert(0, 3 - digits.size(), '0');
    }
    return digits;
}

std::string lookupIso3166Numeric(const std::string& bcdDigits) {
    const std::string key = isoNumericLookupKey(bcdDigits);
    const auto it = g_iso3166.find(key);
    return it != g_iso3166.end() ? it->second : std::string();
}

std::string lookupIso4217Numeric(const std::string& bcdDigits) {
    const std::string key = isoNumericLookupKey(bcdDigits);
    const auto it = g_iso4217.find(key);
    return it != g_iso4217.end() ? it->second : std::string();
}

std::string lookupIsoByEnumRef(const std::string& enumRef, const std::string& bcdDigits) {
    if (enumRef == "iso3166") {
        return lookupIso3166Numeric(bcdDigits);
    }
    if (enumRef == "iso4217") {
        return lookupIso4217Numeric(bcdDigits);
    }
    return {};
}

} // namespace emvparse
