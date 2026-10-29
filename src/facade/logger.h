#ifndef LOGGER_H
#define LOGGER_H

#include "log_types.h"
#include <functional>
#include <string>

/**
 * @brief Log sink (internal).
 *
 * Owned by EmvInputParser::Impl; registries receive a non-owning pointer.
 */
class Logger {
public:
    void setLogCallback(LogCallback callback);

    void log(LogLevel level, const std::string& message) const;

    void error(const std::string& message) const { log(LogLevel::ERROR, message); }
    void warning(const std::string& message) const { log(LogLevel::WARNING, message); }
    void info(const std::string& message) const { log(LogLevel::INFO, message); }

    /** stderr fallback when no instance callback is configured. */
    static Logger& defaultSink();

private:
    LogCallback logCallback_;
};

#endif // LOGGER_H
