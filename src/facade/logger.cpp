#include "logger.h"
#include <iostream>

Logger& Logger::defaultSink() {
    static Logger instance;
    return instance;
}

void Logger::setLogCallback(LogCallback callback) {
    logCallback_ = std::move(callback);
}

void Logger::log(LogLevel level, const std::string& message) const {
    if (logCallback_) {
        logCallback_(level, message);
    } else {
        std::cerr << message << std::endl;
    }
}
