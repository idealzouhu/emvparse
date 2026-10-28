#include "emvparse/apdu_parser.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace {

using json = nlohmann::json;

std::string registryDirname(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    return path.substr(0, pos);
}

std::string joinRegistryPath(const std::string& dir, const std::string& file) {
    if (dir.empty() || dir == ".") {
        return file;
    }
    const char last = dir.back();
    if (last == '/' || last == '\\') {
        return dir + file;
    }
    return dir + "/" + file;
}

bool readJsonFile(const std::string& filePath, json& out) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        Logger::error("Failed to open file: " + filePath);
        return false;
    }

    file.seekg(0, std::ios::end);
    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        Logger::error("File is empty: " + filePath);
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::string content;
    content.resize(fileSize);
    file.read(&content[0], static_cast<std::streamsize>(fileSize));
    file.close();

    if (fileSize >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content = content.substr(3);
    }

    try {
        out = json::parse(content);
    } catch (const json::parse_error& e) {
        Logger::error("JSON parse error in file: " + filePath);
        return false;
    }
    return true;
}

uint8_t parseHexByte(const std::string& hex) {
    return static_cast<uint8_t>(std::stoul(hex, nullptr, 16));
}

uint16_t parseHexWord(const std::string& hex) {
    return static_cast<uint16_t>(std::stoul(hex, nullptr, 16));
}

void loadParamRule(const json& obj, ApduParamRule& rule) {
    if (!obj.is_object()) {
        return;
    }
    if (obj.contains("name") && obj["name"].is_string()) {
        rule.name = obj["name"].get<std::string>();
    }
    if (obj.contains("description") && obj["description"].is_string()) {
        rule.description = obj["description"].get<std::string>();
    }
    if (obj.contains("numericRecord") && obj["numericRecord"].is_boolean()) {
        rule.numericRecord = obj["numericRecord"].get<bool>();
    }
    if (obj.contains("tagReference") && obj["tagReference"].is_boolean()) {
        rule.tagReference = obj["tagReference"].get<bool>();
    }
    if (obj.contains("sfiShift") && obj["sfiShift"].is_number_integer()) {
        rule.sfiShift = obj["sfiShift"].get<int>();
    }
    if (obj.contains("enumMap") && obj["enumMap"].is_object()) {
        for (auto it = obj["enumMap"].begin(); it != obj["enumMap"].end(); ++it) {
            if (it.value().is_string()) {
                std::string key = it.key();
                for (char& c : key) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
                rule.enumMap[key] = it.value().get<std::string>();
            }
        }
    }
}

} // namespace

bool ApduRegistry::loadFromManifest(const std::string& manifestPath) {
    commands_.clear();
    commandIndex_.clear();
    statusWords_.clear();

    json manifest;
    if (!readJsonFile(manifestPath, manifest)) {
        return false;
    }
    if (!manifest.is_object() || !manifest.contains("apduCommands") ||
        !manifest["apduCommands"].is_string()) {
        Logger::warning("Manifest has no apduCommands entry; APDU naming disabled");
        return true;
    }

    const std::string dir = registryDirname(manifestPath);
    const std::string apduPath =
        joinRegistryPath(dir, manifest["apduCommands"].get<std::string>());

    json root;
    if (!readJsonFile(apduPath, root)) {
        return false;
    }

    if (root.contains("commands") && root["commands"].is_array()) {
        for (const auto& cmdJson : root["commands"]) {
            if (!cmdJson.is_object() || !cmdJson.contains("cla") || !cmdJson.contains("ins")) {
                continue;
            }
            ApduCommandInfo cmd;
            cmd.cla = parseHexByte(cmdJson["cla"].get<std::string>());
            cmd.ins = parseHexByte(cmdJson["ins"].get<std::string>());
            if (cmdJson.contains("name") && cmdJson["name"].is_string()) {
                cmd.name = cmdJson["name"].get<std::string>();
            }
            if (cmdJson.contains("dataFormat") && cmdJson["dataFormat"].is_string()) {
                cmd.dataFormat = cmdJson["dataFormat"].get<std::string>();
            }
            if (cmdJson.contains("p1")) {
                loadParamRule(cmdJson["p1"], cmd.p1);
            }
            if (cmdJson.contains("p2")) {
                loadParamRule(cmdJson["p2"], cmd.p2);
            }

            const uint32_t key =
                (static_cast<uint32_t>(cmd.cla) << 8) | static_cast<uint32_t>(cmd.ins);
            commandIndex_[key] = commands_.size();
            commands_.push_back(cmd);
        }
    }

    if (root.contains("statusWords") && root["statusWords"].is_array()) {
        for (const auto& swJson : root["statusWords"]) {
            if (!swJson.is_object() || !swJson.contains("sw")) {
                continue;
            }
            const uint16_t sw = parseHexWord(swJson["sw"].get<std::string>());
            std::string meaning;
            if (swJson.contains("meaning") && swJson["meaning"].is_string()) {
                meaning = swJson["meaning"].get<std::string>();
            }
            statusWords_[sw] = meaning;
        }
    }

    Logger::info(
        "APDU registry loaded: " + std::to_string(commands_.size()) + " commands, " +
        std::to_string(statusWords_.size()) + " status words");
    return true;
}

const ApduCommandInfo* ApduRegistry::findCommand(uint8_t cla, uint8_t ins) const {
    const uint32_t key = (static_cast<uint32_t>(cla) << 8) | static_cast<uint32_t>(ins);
    const auto it = commandIndex_.find(key);
    if (it == commandIndex_.end()) {
        return nullptr;
    }
    return &commands_[it->second];
}

std::string ApduRegistry::lookupStatusWord(uint16_t sw) const {
    const auto it = statusWords_.find(sw);
    if (it != statusWords_.end()) {
        return it->second;
    }
    return "";
}

bool ApduRegistry::isKnownStatusPattern(uint16_t sw) const {
    if (!lookupStatusWord(sw).empty()) {
        return true;
    }
    const uint8_t hi = static_cast<uint8_t>((sw >> 8) & 0xFF);
    const uint8_t lo = static_cast<uint8_t>(sw & 0xFF);
    (void)lo;
    if (hi == 0x61) {
        return true;
    }
    if (hi == 0x6C) {
        return true;
    }
    if (hi >= 0x62 && hi <= 0x6F) {
        return true;
    }
    if (hi >= 0x90 && hi <= 0x9F) {
        return true;
    }
    return false;
}
