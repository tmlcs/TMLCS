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
#include "stdarg.h" /* For va_list, va_start, va_end (manual implementation) */
#include "string.h"

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* Buffer sizes */
#define LOG_BUFFER_SIZE 256

/* =============================================================================
 * CRIT-SEC-001 FIX: Format String Validation
 * =============================================================================
 * Validate and sanitize format strings to prevent format string attacks.
 * This provides defense-in-depth even if a developer accidentally passes
 * user-controlled data as the format string.
 * =============================================================================
 */

/**
 * @brief Check if a character is a valid format specifier
 * @param c Character to check
 * @return 1 if valid format specifier, 0 otherwise
 */
static int is_valid_format_spec(char c) {
    /* Supported specifiers: %d, %u, %x, %X, %s, %c, %%, %p, %lld, %llu, %llx, %zu */
    return (c == 'd' || c == 'u' || c == 'x' || c == 'X' || c == 's' || c == 'c' || c == '%' ||
            c == 'p');
}

/**
 * @brief Validate format string for security
 * @param fmt Format string to validate
 * @return 1 if valid, 0 if suspicious/potentially dangerous
 *
 * Checks for:
 * - Excessive format specifiers (potential stack read)
 * - %n specifier (write attack)
 * - Unbalanced format specifiers
 */
static int validate_format_string(const char* fmt) {
    if (fmt == nullptr) {
        return 0;
    }

    int format_count = 0;
    const int MAX_FORMAT_COUNT = 20; /* Reasonable upper limit */

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (!*fmt) {
                return 0; /* Trailing % is invalid */
            }

            /* CRIT-SEC-001 FIX: Reject %n (write attack vector) */
            if (*fmt == 'n') {
                return 0; /* %n is never safe */
            }

            /* Skip length modifiers for counting */
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l')
                    fmt++; /* %ll */
            } else if (*fmt == 'z') {
                fmt++; /* %z */
            }

            if (!*fmt) {
                return 0; /* Trailing % after modifier */
            }

            /* Count valid format specifiers */
            if (is_valid_format_spec(*fmt)) {
                format_count++;
                if (format_count > MAX_FORMAT_COUNT) {
                    return 0; /* Too many specifiers - potential attack */
                }
            } else {
                /* Unknown specifier - could be %n variant or attack */
                return 0;
            }
        }
        fmt++;
    }

    return 1; /* Format string appears safe */
}

/**
 * @brief Sanitize a string by escaping % characters
 * @param dst Destination buffer
 * @param dst_size Destination buffer size
 * @param src Source string (potentially untrusted)
 * @return Number of characters written (excluding null)
 *
 * This prevents user-controlled strings from being interpreted as
 * format specifiers when logged with %s.
 */
static size_t sanitize_user_string(char* dst, size_t dst_size, const char* src) {
    if (dst == nullptr || dst_size == 0 || src == nullptr) {
        return 0;
    }

    size_t written = 0;
    size_t i = 0;

    while (src[i] != '\0' && written < dst_size - 1) {
        if (src[i] == '%') {
            /* Escape % as %% to prevent interpretation as format specifier */
            /* Need 2 chars for %% plus 1 for \0, so break if written + 3 > dst_size */
            if (written + 3 > dst_size) {
                break; /* Not enough space */
            }
            dst[written++] = '%';
            dst[written++] = '%';
        } else {
            dst[written++] = src[i];
        }
        i++;
    }

    dst[written] = '\0';
    return written;
}

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

/* Helper: append unsigned 64-bit decimal */
static void append_dec64(char** buf, uint64_t val, const char* buf_end) {
    if (val == 0) {
        append_char(buf, '0', buf_end);
        return;
    }
    char tmp[24]; /* max 20 digits for uint64_t */
    int i = 0;
    while (val > 0) {
        tmp[i++] = '0' + (int) (val % 10);
        val /= 10;
    }
    while (i > 0) {
        append_char(buf, tmp[--i], buf_end);
    }
}

/* Helper: append signed 64-bit decimal */
static void append_dec64_signed(char** buf, int64_t val, const char* buf_end) {
    if (val < 0) {
        append_char(buf, '-', buf_end);
        /* Safe negate: avoids UB on INT64_MIN by widening before negation */
        append_dec64(buf, (uint64_t) (-(val + 1)) + 1, buf_end);
    } else {
        append_dec64(buf, (uint64_t) val, buf_end);
    }
}

/* Helper: append 64-bit hex, always zero-padded to 16 digits.
 * LOW-NEW-002 FIX: Variable-width output made %p unreadable for addresses
 * with leading zero nibbles (e.g. 0x1234 instead of 0x0000000000001234).
 * Emitting exactly 16 digits makes pointer values easy to compare. */
static void append_hex64(char** buf, uint64_t val, const char* buf_end) {
    append_string(buf, "0x", buf_end);
    for (int shift = 60; shift >= 0; shift -= 4) {
        int d = (int) ((val >> shift) & 0xF);
        append_char(buf, (d < 10) ? ('0' + d) : ('A' + d - 10), buf_end);
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
            /* Safe negation: avoids INT32_MIN overflow UB */
            append_dec(buf, (uint32_t) (-(val + 1)) + 1, buf_end);
        } else {
            append_dec(buf, (uint32_t) val, buf_end);
        }
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
            /* LOW-002 FIX: Handle multi-char format specifiers %ll*, %z*, %p */
            if (*fmt == 'l' && *(fmt + 1) == 'l') {
                fmt += 2; /* skip "ll" */
                if (!*fmt)
                    break;
                switch (*fmt) {
                case 'd':
                    append_dec64_signed(&buf, va_arg(args, int64_t), buf_end);
                    break;
                case 'u':
                    append_dec64(&buf, va_arg(args, uint64_t), buf_end);
                    break;
                case 'x':
                    append_hex64(&buf, va_arg(args, uint64_t), buf_end);
                    break;
                default:
                    append_char(&buf, '%', buf_end);
                    append_string(&buf, "ll", buf_end);
                    append_char(&buf, *fmt, buf_end);
                    break;
                }
            } else if (*fmt == 'z' && *(fmt + 1) == 'u') {
                fmt++; /* skip 'z', loop will advance past 'u' */
                append_dec64(&buf, (uint64_t) va_arg(args, size_t), buf_end);
            } else if (*fmt == 'p') {
                append_hex64(&buf, (uint64_t) (uintptr_t) va_arg(args, void*), buf_end);
            } else {
                format_append_arg(&buf, *fmt, args, buf_end);
            }
        } else {
            append_char(&buf, *fmt, buf_end);
        }
        fmt++;
    }

    /* LOW-003 FIX: Append "..." truncation marker when the format string was
     * not fully consumed.  Overwrites the last 3 characters so the NUL slot
     * is always preserved. */
    if (*fmt != '\0' && message_size >= 4) {
        char* mark = buf_end - 3; /* positions: buf_end-3, buf_end-2, buf_end-1; NUL at buf_end */
        if (mark < message)
            mark = message;
        mark[0] = '.';
        if (mark + 1 < buf_end)
            mark[1] = '.';
        if (mark + 2 < buf_end)
            mark[2] = '.';
        buf = (mark + 3 <= buf_end) ? mark + 3 : buf_end;
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

    /* CRIT-SEC-001 FIX: Validate format string before processing */
    if (!validate_format_string(fmt)) {
        /* Format string validation failed - output error instead */
        if (serial_is_initialized() != 0) {
            serial_write_str("[LOG SECURITY] Invalid format string rejected\r\n");
        }
        return;
    }

    /* Acquire lock for thread safety */
    spinlock_token_t tok = spinlock_acquire(&g_log_lock);

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

    spinlock_release(&g_log_lock, tok);

    /* Handle PANIC level - halt system */
    if (level == LOG_LEVEL_PANIC) {
        __asm__ volatile("cli" ::: "memory"); /* Disable interrupts before halt */
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

    /* CRIT-SEC-001 FIX: Validate format string before processing */
    if (!validate_format_string(fmt)) {
        /* Format string validation failed - output error instead */
        if (serial_is_initialized() != 0) {
            serial_write_str("[LOG SECURITY] Invalid format string rejected\r\n");
        }
        return;
    }

    /* Acquire lock for thread safety */
    spinlock_token_t tok = spinlock_acquire(&g_log_lock);

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

    spinlock_release(&g_log_lock, tok);
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
    LOG_DEBUG("%s (%u bytes):", label, (uint32_t) len);
    (void) label; /* Suppress unused-parameter warning when LOG_DEBUG is compiled out */

    spinlock_token_t tok = spinlock_acquire(&g_log_lock);

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(addr);

    for (size_t i = 0; i < len; i += 16) {
        char output[80];
        char* out = output;
        char* out_end = output + sizeof(output) - 1;
        output[0] = '\0';

        /* Offset */
        append_string(&out, "  ", out_end);
        append_hex64(&out, (uint64_t) ((uintptr_t) bytes + i), out_end);
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

    spinlock_release(&g_log_lock, tok);
}

/* =============================================================================
 * log_format_to_buf — public test helper
 * Wraps build_message so tests can verify truncation without serial interception.
 * ============================================================================= */
size_t log_format_to_buf(char* dst, size_t cap, const char* fmt, ...) {
    if (!dst || cap == 0) {
        return 0;
    }
    if (cap == 1) {
        dst[0] = '\0';
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    build_message(dst, cap, fmt, args);
    va_end(args);
    return strlen(dst);
}

/* =============================================================================
 * CRIT-SEC-001 FIX: Public Format String Security API
 * =============================================================================
 */

size_t log_sanitize_string(char* dst, size_t dst_size, const char* src) {
    return sanitize_user_string(dst, dst_size, src);
}

int log_validate_format(const char* fmt) {
    return validate_format_string(fmt);
}
