#ifndef EMVPARSE_LOGGING_H
#define EMVPARSE_LOGGING_H

#include <functional>
#include <string>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

#endif // EMVPARSE_LOGGING_H
