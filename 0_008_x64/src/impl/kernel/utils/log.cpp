#include "utils/log.h"
#include "drivers/serial.h"
#include "print.h"
#include <stdint.h> // Required for uint64_t
#include <stdarg.h> // Required for va_list

namespace Log {

    const char* LogLevelStrings[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "CRITICAL"
    };

        // Temporary helper to convert uint64_t to string for logging

        // This is a very basic implementation and should be replaced by a proper library function

        static char s_buffer[32]; // Increased buffer size for hexadecimal representation (max 16 hex digits + "0x" + null terminator)

        const char* uint64_to_string(uint64_t value, uint8_t base) {

            if (value == 0) return "0";

    

            const char* digits = "0123456789ABCDEF";

            int i = 31;

            s_buffer[i--] = '\0';

    

            if (base == 0) base = 10; // Default to decimal if base is 0

    

            while (value > 0 && i >= 0) {

                s_buffer[i--] = digits[value % base];

                value /= base;

            }

    

            if (base == 16) { // Add "0x" prefix for hexadecimal

                s_buffer[i--] = 'x';

                s_buffer[i--] = '0';

            }

            return &s_buffer[i + 1];

        }

    

        void vprintf(LogLevel level, const char* format, va_list args) {

            Serial::write_string("[");

            Serial::write_string(LogLevelStrings[level]);

            Serial::write_string("] ");

    

            if (level >= ERROR) {

                print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);

                print_str("[");

                print_str(LogLevelStrings[level]);

                print_str("] ");

            }

    

            const char* current_format = format;

            while (*current_format != '\0') {

                if (*current_format == '%') {

                    current_format++;

                    if (*current_format == '\0') break; // Malformed format string

    

                    if (*current_format == 's') {

                        const char* s = va_arg(args, const char*);

                        Serial::write_string(s);

                        if (level >= ERROR) print_str(s);

                    } else if (*current_format == 'd') {

                        uint64_t d = va_arg(args, uint64_t);

                        const char* s = uint64_to_string(d, 10);

                        Serial::write_string(s);

                        if (level >= ERROR) print_str(s);

                    } else if (*current_format == 'x') {

                        uint64_t x = va_arg(args, uint64_t);

                        const char* s = uint64_to_string(x, 16);

                        Serial::write_string(s);

                        if (level >= ERROR) print_str(s);

                    } else {

                        // Unknown format specifier, print it literally

                        Serial::write_char('%');

                        Serial::write_char(*current_format);

                        if (level >= ERROR) {

                            print_char('%');

                            print_char(*current_format);

                        }

                    }

                } else {

                    Serial::write_char(*current_format);

                    if (level >= ERROR) print_char(*current_format);

                }

                current_format++;

            }

            Serial::write_string("\n");

            if (level >= ERROR) print_str("\n");

        }

    

        void debug(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(DEBUG, format, args);

            va_end(args);

        }

    

        void info(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(INFO, format, args);

            va_end(args);

        }

    

        void warning(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(WARNING, format, args);

            va_end(args);

        }

    

        void error(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(ERROR, format, args);

            va_end(args);

        }

    

        void critical(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(CRITICAL, format, args);

            va_end(args);

        }

    

        void panic(const char* format, ...) {

            va_list args;

            va_start(args, format);

            vprintf(CRITICAL, format, args); // Log the critical message

            va_end(args);

    

            // Halt the CPU indefinitely

            asm volatile ("cli"); // Disable interrupts

            for(;;) {

                asm volatile ("hlt");

            }

        }

    }

    

