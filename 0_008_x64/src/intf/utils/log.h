#ifndef LOG_H
#define LOG_H

#include <stdint.h> // Required for uint64_t
#include <stdarg.h> // Required for va_list

namespace Log {
    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    void vprintf(LogLevel level, const char* format, va_list args);
    void debug(const char* format, ...);
    void info(const char* format, ...);
    void warning(const char* format, ...);
    void error(const char* format, ...);
    void critical(const char* format, ...);
    void panic(const char* format, ...);
}

#endif // LOG_H
