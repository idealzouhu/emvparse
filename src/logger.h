#ifndef LOGGER_H
#define LOGGER_H

#include <emvparse/logging.h>
#include <functional>
#include <string>

/**
 * @brief Log manager (internal).
 *
 * Centralizes library logging and optional user callbacks.
 * Components log through Logger instead of duplicating output logic.
 */
class Logger {
public:
    // Singleton instance.
    static Logger& getInstance();

    // Register log callback.
    void setLogCallback(LogCallback callback);

    // Emit a log line.
    void log(LogLevel level, const std::string& message) const;

    // Convenience wrappers around getInstance().log().
    static void error(const std::string& message);
    static void warning(const std::string& message);
    static void info(const std::string& message);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogCallback logCallback_;
};

#endif // LOGGER_H
