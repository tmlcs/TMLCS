/*
 * GLOBEX_OS Logging System Implementation
 * =======================================
 *
 * Multi-level logging with serial and VGA output.
 * Thread-safe via spinlock protection.
 */

#include "log.h"
#include "print.h"
#include "serial.h"
#include "spinlock.h"
#include "string.h"
#include "stdarg.h"  /* For va_list, va_start, va_end (manual implementation) */

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* Buffer sizes */
#define LOG_BUFFER_SIZE 256

/* =============================================================================
 * Global State
 * =============================================================================
 */

static int g_log_initialized = 0;
static int g_log_level = LOG_LEVEL_DEBUG; /* Runtime log level */
static spinlock_t g_log_lock = SPINLOCK_INIT;

/* =============================================================================
 * Level Name Lookup
 * =============================================================================
 */

static const char* get_level_name(int level) {
    switch (level) {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARN:
        return "WARN";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_PANIC:
        return "PANIC";
    default:
        return "UNKNOWN";
    }
}

/* =============================================================================
 * Internal Helper Functions - Buffer Operations
 * =============================================================================
 */

/* Helper: append char with bounds check */
static void append_char(char** buf, char c, const char* buf_end) {
    if (*buf < buf_end) {
        **buf = c;
        (*buf)++;
    }
}

/* Helper: append string */
static void append_string(char** buf, const char* str, const char* buf_end) {
    while (*str && *buf < buf_end) {
        append_char(buf, *str, buf_end);
        str++;
    }
}

/* Helper: append decimal */
static void append_dec(char** buf, uint32_t val, const char* buf_end) {
    if (val == 0) {
        append_char(buf, '0', buf_end);
        return;
    }
    char tmp[16];
    int i = 0;
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        append_char(buf, tmp[--i], buf_end);
    }
}

/* Helper: append hex */
static void append_hex(char** buf, uint32_t val, const char* buf_end) {
    append_string(buf, "0x", buf_end);
    if (val == 0) {
        append_char(buf, '0', buf_end);
        return;
    }
    char tmp[16];
    int i = 0;
    while (val > 0) {
        int d = val & 0xF;
        tmp[i++] = (d < 10) ? ('0' + d) : ('A' + d - 10);
        val >>= 4;
    }
    while (i > 0) {
        append_char(buf, tmp[--i], buf_end);
    }
}

/* =============================================================================
 * Internal Helper Functions - Message Formatting
 * =============================================================================
 */

/**
 * @brief Format a message string from format specifier and va_list
 * @param buf Pointer to buffer pointer
 * @param fmt Format specifier character (d, u, x, s, c, %)
 * @param args Variable argument list
 * @param buf_end End of buffer for bounds checking
 * @return void
 *
 * @note Advances fmt pointer internally - caller must manage fmt
 */
static void format_append_arg(char** buf, char fmt_spec, va_list args, const char* buf_end) {
    switch (fmt_spec) {
    case 'd': {
        int32_t val = va_arg(args, int32_t);
        if (val < 0) {
            append_char(buf, '-', buf_end);
            val = -val;
        }
        append_dec(buf, (uint32_t) val, buf_end);
        break;
    }
    case 'u':
        append_dec(buf, va_arg(args, uint32_t), buf_end);
        break;
    case 'x':
        append_hex(buf, va_arg(args, uint32_t), buf_end);
        break;
    case 's': {
        const char* str = va_arg(args, const char*);
        if (str != nullptr) {
            append_string(buf, str, buf_end);
        } else {
            append_string(buf, "(null)", buf_end);
        }
        break;
    }
    case 'c':
        append_char(buf, (char) va_arg(args, int), buf_end);
        break;
    case '%':
        append_char(buf, '%', buf_end);
        break;
    default:
        append_char(buf, '%', buf_end);
        append_char(buf, fmt_spec, buf_end);
        break;
    }
}

/**
 * @brief Build formatted message from format string and arguments
 * @param message Output buffer
 * @param message_size Size of output buffer
 * @param fmt Format string
 * @param args Variable argument list
 * @return void
 *
 * @note Ensures null-termination even on truncation
 */
static void build_message(char* message, size_t message_size, const char* fmt, va_list args) {
    char* buf = message;
    char* buf_end = message + message_size - 1;

    message[0] = '\0';

    while (*fmt && buf < buf_end) {
        if (*fmt == '%') {
            fmt++;
            if (!*fmt) {
                break;
            }
            format_append_arg(&buf, *fmt, args, buf_end);
        } else {
            append_char(&buf, *fmt, buf_end);
        }
        fmt++;
    }
    *buf = '\0';
}

/* =============================================================================
 * Internal Helper Functions - Output Building
 * =============================================================================
 */

/**
 * @brief Extract filename from full path
 * @param file Full file path
 * @return Pointer to filename within path (or same pointer if no path)
 *
 * @note Returns pointer into original string, not a copy
 */
static const char* extract_filename(const char* file) {
    const char* filename = file;
    const char* p = file;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }
    return filename;
}

/**
 * @brief Build output string with level, module, location, and message
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @param level Log level
 * @param module Module name
 * @param filename Source filename (without path)
 * @param line Source line number
 * @param message Formatted message
 * @return void
 */
static void build_output_string(char* output, size_t output_size, int level, const char* module,
                                const char* filename, int line, const char* message) {
    char* out = output;
    char* out_end = output + output_size - 1;

    output[0] = '\0';

    /* Level and module: [LEVEL][MODULE] */
    append_char(&out, '[', out_end);
    append_string(&out, get_level_name(level), out_end);
    append_char(&out, ']', out_end);
    append_char(&out, '[', out_end);
    append_string(&out, module, out_end);
    append_char(&out, ']', out_end);
    append_char(&out, ' ', out_end);

    /* Location info: file:line */
    append_string(&out, filename, out_end);
    append_char(&out, ':', out_end);
    append_dec(&out, (uint32_t) line, out_end);
    append_char(&out, ':', out_end);
    append_char(&out, ' ', out_end);

    /* Message */
    append_string(&out, message, out_end);
    append_string(&out, "\r\n", out_end);
    *out = '\0';
}

/**
 * @brief Build simple output string (level + message only)
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @param level Log level
 * @param message Formatted message
 * @return void
 */
static void build_simple_output_string(char* output, size_t output_size, int level,
                                       const char* message) {
    char* out = output;
    char* out_end = output + output_size - 1;

    output[0] = '\0';

    append_char(&out, '[', out_end);
    append_string(&out, get_level_name(level), out_end);
    append_string(&out, "] ", out_end);
    append_string(&out, message, out_end);
    append_string(&out, "\r\n", out_end);
    *out = '\0';
}

/* =============================================================================
 * Internal Helper Functions - Output Writing
 * =============================================================================
 */

/**
 * @brief Write output string to serial and optionally VGA
 * @param output String to write
 * @param use_vga If true and VGA available, also write to VGA
 * @return void
 */
static void write_output(const char* output, bool use_vga) {
    /* Output to serial */
    if (serial_is_initialized() != 0) {
        serial_write_str(output);
    }

    /* Output to VGA if enabled */
    if (use_vga && print_is_initialized() != 0) {
        print_str(output);
    }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

int log_init(void) {
    g_log_initialized = 1;
    g_log_level = LOG_LEVEL_DEBUG;

    LOG_INFO("Logging system initialized");
    return 1;
}

void log_set_level(int level) {
    if (level < LOG_LEVEL_DEBUG) {
        level = LOG_LEVEL_DEBUG;
    } else if (level > LOG_LEVEL_PANIC) {
        level = LOG_LEVEL_PANIC;
    }
    g_log_level = level;
}

int log_get_level(void) {
    return g_log_level;
}

int log_is_initialized(void) {
    return g_log_initialized;
}

/* =============================================================================
 * Internal Log Output (with location info)
 * =============================================================================
 *
 * SECURITY WARNING:
 *   The `fmt` parameter MUST be a string literal, NEVER user-controlled input.
 *   See log.h for detailed security documentation.
 *
 * Format String Vulnerability Prevention:
 *   - fmt: Only accept string literals (e.g., "Value: %d")
 *   - Never: Pass variables or external input as fmt
 *   - Use: %s specifier to safely log untrusted strings as DATA
 *
 * Example SAFE usage:
 *   LOG_INFO("User input: %s", user_data);  // user_data is treated as DATA
 *
 * Example UNSAFE usage (DO NOT DO THIS):
 *   log_output(LOG_LEVEL_INFO, ..., user_data);  // VULNERABILITY!
 */

void log_output(int level, const char* module, const char* file, int line, const char* fmt, ...) {
    /* Check log level - only output if level is at or above configured level */
    if (level < g_log_level || g_log_initialized == 0) {
        return;
    }

    /* Acquire lock for thread safety */
    spinlock_acquire(&g_log_lock);

    /* Build formatted message */
    char message[LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    build_message(message, sizeof(message), fmt, args);
    va_end(args);

    /* Extract filename from path */
    const char* filename = extract_filename(file);

    /* Build output string with location info */
    char output[LOG_BUFFER_SIZE + 64];
    build_output_string(output, sizeof(output), level, module, filename, line, message);

    /* Write to serial and VGA */
    write_output(output, true);

    spinlock_release(&g_log_lock);

    /* Handle PANIC level - halt system */
    if (level == LOG_LEVEL_PANIC) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
}

/* =============================================================================
 * Internal Log Output (simple, no location info)
 * =============================================================================
 *
 * SECURITY WARNING:
 *   The `fmt` parameter MUST be a string literal, NEVER user-controlled input.
 *   See log.h for detailed security documentation.
 *
 * Format String Vulnerability Prevention:
 *   - fmt: Only accept string literals (e.g., "Value: %d")
 *   - Never: Pass variables or external input as fmt
 *   - Use: %s specifier to safely log untrusted strings as DATA
 */

void log_output_simple(int level, const char* fmt, ...) {
    /* Check log level */
    if (level < g_log_level || g_log_initialized == 0) {
        return;
    }

    /* Acquire lock for thread safety */
    spinlock_acquire(&g_log_lock);

    /* Build formatted message */
    char message[LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    build_message(message, sizeof(message), fmt, args);
    va_end(args);

    /* Build simple output string */
    char output[LOG_BUFFER_SIZE + 32];
    build_simple_output_string(output, sizeof(output), level, message);

    /* Write to serial and VGA (no cursor management needed) */
    write_output(output, true);

    spinlock_release(&g_log_lock);
}

/* =============================================================================
 * Hex Dump Implementation
 * =============================================================================
 */

void log_hex_dump(const char* label, const void* addr, size_t len) {
    if (g_log_initialized == 0) {
        return;
    }

    /* Print label before acquiring lock — LOG_DEBUG internally acquires
     * g_log_lock; calling it while the lock is held would self-deadlock. */
    LOG_DEBUG("%s (%u bytes):", label, (uint32_t)len);

    spinlock_acquire(&g_log_lock);

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(addr);

    for (size_t i = 0; i < len; i += 16) {
        char output[80];
        char* out = output;
        char* out_end = output + sizeof(output) - 1;
        output[0] = '\0';

        /* Offset */
        append_string(&out, "  ", out_end);
        append_hex(&out, (uint32_t) (i + (size_t) bytes), out_end);
        append_string(&out, ":  ", out_end);

        /* Hex bytes */
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            if (j == 8) {
                append_string(&out, " ", out_end);
            }
            append_hex(&out, bytes[i + j], out_end);
            append_string(&out, " ", out_end);
        }

        /* Pad to align ASCII */
        size_t padding = (len - i) < 16 ? (len - i) : 16;
        for (size_t j = padding; j < 16; j++) {
            append_string(&out, "   ", out_end);
            if (j == 7) {
                append_string(&out, " ", out_end);
            }
        }

        append_string(&out, "  |", out_end);

        /* ASCII representation */
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t c = bytes[i + j];
            if (c >= 32 && c < 127) {
                append_char(&out, c, out_end);
            } else {
                append_char(&out, '.', out_end);
            }
        }

        append_string(&out, "|", out_end);
        *out = '\0';

        /* Write line to output */
        if (serial_is_initialized() != 0) {
            serial_write_str(output);
            serial_write_str("\r\n");
        }

#if LOG_TO_VGA
        if (print_is_initialized()) {
            print_str(output);
            print_str("\r\n");
        }
#endif
    }

    spinlock_release(&g_log_lock);
}
