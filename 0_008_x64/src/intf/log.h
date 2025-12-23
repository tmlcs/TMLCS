#ifndef LOG_H
#define LOG_H

#include <stdint.h> // Required for uint64_t

namespace Log {
    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    void write(LogLevel level, const char* message);
    void debug(const char* message);
    void info(const char* message);
    void warning(const char* message);
    void error(const char* message);
    void critical(const char* message);
    const char* uint64_to_string(uint64_t value);
}

#endif // LOG_H
