#include "log.h"
#include "serial.h"
#include "print.h"
#include <stdint.h> // Required for uint64_t

namespace Log {

    const char* LogLevelStrings[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "CRITICAL"
    };

    void write(LogLevel level, const char* message) {
        // Print to serial
        Serial::write_string("[");
        Serial::write_string(LogLevelStrings[level]);
        Serial::write_string("] ");
        Serial::write_string(message);
        Serial::write_string("\n");

        // Print to VGA (only for ERROR and CRITICAL for now to avoid clutter)
        if (level >= ERROR) {
            print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
            print_str("[");
            print_str(LogLevelStrings[level]);
            print_str("] ");
            print_str(message);
            print_str("\n");
        }
    }

    void debug(const char* message) {
        write(DEBUG, message);
    }

    void info(const char* message) {
        write(INFO, message);
    }

    void warning(const char* message) {
        write(WARNING, message);
    }

    void error(const char* message) {
        write(ERROR, message);
    }

    void critical(const char* message) {
        write(CRITICAL, message);
    }

    // Temporary helper to convert uint64_t to string for logging
    // This is a very basic implementation and should be replaced by a proper library function
    static char s_buffer[20]; // Max 20 digits for uint64_t
    const char* uint64_to_string(uint64_t value) {
        if (value == 0) return "0";
        int i = 19;
        s_buffer[i--] = '\0';
        while (value > 0 && i >= 0) {
            s_buffer[i--] = (value % 10) + '0';
            value /= 10;
        }
        return &s_buffer[i + 1];
    }
}
