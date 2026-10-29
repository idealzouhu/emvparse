#include "hex_util.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

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

#ifdef _WIN32
static void ensureStdoutUnbuffered() {
    static const bool done = [] {
        (void)_setmode(_fileno(stdout), _O_BINARY);
        (void)setvbuf(stdout, nullptr, _IONBF, 0);
        return true;
    }();
    (void)done;
}

/** ConPTY / integrated terminals may truncate large iostream writes; WriteFile per line is safer. */
static bool writeTextToWindowsConsole(const std::string& content) {
    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return false;
    }
    DWORD consoleMode = 0;
    if (!GetConsoleMode(hOut, &consoleMode)) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        const std::size_t next = content.find('\n', offset);
        const std::size_t end = (next == std::string::npos) ? content.size() : next + 1;
        const std::size_t n = end - offset;
        DWORD written = 0;
        if (!WriteFile(hOut, content.data() + static_cast<std::ptrdiff_t>(offset),
                       static_cast<DWORD>(n), &written, nullptr) ||
            written != n) {
            return false;
        }
        offset = end;
    }
    return true;
}
#endif

void writeTextToStdout(const std::string& content) {
#ifdef _WIN32
    ensureStdoutUnbuffered();
    if (writeTextToWindowsConsole(content)) {
        return;
    }
#endif
    constexpr std::size_t kChunk = 4096;
    for (std::size_t offset = 0; offset < content.size(); offset += kChunk) {
        const std::size_t n = (std::min)(kChunk, content.size() - offset);
        std::cout.write(content.data() + static_cast<std::ptrdiff_t>(offset),
                        static_cast<std::streamsize>(n));
        if (!std::cout.good()) {
            break;
        }
        std::cout.flush();
    }
    if (!content.empty() && content.back() != '\n') {
        std::cout << '\n';
    }
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
