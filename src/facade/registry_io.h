#ifndef EMVPARSE_REGISTRY_IO_H
#define EMVPARSE_REGISTRY_IO_H

// Shared manifest/JSON loading for TagRegistry and ApduRegistry (UTF-8 BOM, parse errors → log).

#include <nlohmann/json_fwd.hpp>
#include <string>

class Logger;

std::string registryDirname(const std::string& path);
std::string joinRegistryPath(const std::string& dir, const std::string& file);
bool readRegistryJsonFile(const std::string& filePath, nlohmann::json& out, Logger& log);

#endif // EMVPARSE_REGISTRY_IO_H
