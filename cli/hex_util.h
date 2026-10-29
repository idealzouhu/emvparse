#ifndef EMVPARSE_CLI_HEX_UTIL_H
#define EMVPARSE_CLI_HEX_UTIL_H

#include <string>
#include <vector>
#include <cstdint>

std::string normalizeHexInput(std::string input);

bool readHexFromFile(const std::string& path, std::string& hexOut, std::string& errorOut);

bool readHexFromStdin(std::string& hexOut, std::string& errorOut);

bool writeTextToFile(const std::string& path, const std::string& content, std::string& errorOut);

/** Writes to stdout in chunks (helps ConPTY/integrated terminals with large script traces). */
void writeTextToStdout(const std::string& content);

std::string defaultHtmlReportPath(const std::string& hexSourcePath);

#endif
