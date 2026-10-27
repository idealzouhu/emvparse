#include "logger.h"
#include <iostream>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

void Logger::log(LogLevel level, const std::string& message) const {
    if (logCallback_) {
        logCallback_(level, message);
    } else {
        // No callback: fall back to stderr.
        std::cerr << message << std::endl;
    }
}

void Logger::error(const std::string& message) {
    getInstance().log(LogLevel::ERROR, message);
}

void Logger::warning(const std::string& message) {
    getInstance().log(LogLevel::WARNING, message);
}

void Logger::info(const std::string& message) {
    getInstance().log(LogLevel::INFO, message);
}

