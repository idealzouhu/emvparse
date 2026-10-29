#include "registry_io.h"
#include "logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

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

bool readRegistryJsonFile(const std::string& filePath, nlohmann::json& out, Logger& log) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        log.error("Failed to open file: " + filePath);
        return false;
    }

    file.seekg(0, std::ios::end);
    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        log.error("File is empty: " + filePath);
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
        out = nlohmann::json::parse(content);
    } catch (const nlohmann::json::parse_error& e) {
        log.error("JSON parse error in file: " + filePath);
        const char* whatMsg = e.what();
        if (whatMsg && *whatMsg != '\0') {
            log.error(std::string(whatMsg));
        }
        return false;
    }
    return true;
}
