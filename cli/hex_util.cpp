#include "hex_util.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

std::string normalizeHexInput(std::string input) {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    if (input.size() >= 2 && (input[0] == '0' || input[0] == '0') &&
        (input[1] == 'x' || input[1] == 'X')) {
        i = 2;
    }
    for (; i < input.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (std::isspace(c)) {
            continue;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

static std::string stripFileContent(const std::string& raw) {
    return normalizeHexInput(raw);
}

bool readHexFromFile(const std::string& path, std::string& hexOut, std::string& errorOut) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "cannot open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    hexOut = stripFileContent(ss.str());
    return true;
}

bool readHexFromStdin(std::string& hexOut, std::string& errorOut) {
    (void)errorOut;
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    hexOut = stripFileContent(ss.str());
    return true;
}

std::string defaultHtmlReportPath(const std::string& hexSourcePath) {
    if (hexSourcePath.empty()) {
        return "report.html";
    }
    const std::size_t slash = hexSourcePath.find_last_of("/\\");
    const std::size_t dot = hexSourcePath.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return hexSourcePath.substr(0, dot) + ".html";
    }
    return hexSourcePath + ".html";
}

bool writeTextToFile(const std::string& path, const std::string& content, std::string& errorOut) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        errorOut = "cannot write file: " + path;
        return false;
    }
    out << content;
    if (!out.good()) {
        errorOut = "write failed: " + path;
        return false;
    }
    return true;
}
