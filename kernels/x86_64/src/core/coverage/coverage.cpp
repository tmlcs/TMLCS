/*
 * GLOBEX_OS Code Coverage Implementation
 * =======================================
 *
 * Manual coverage tracking for freestanding kernel.
 * This module tracks function execution and outputs coverage data.
 */

#include "coverage.h"
#include "serial.h"
#include "string.h"

// Global coverage table
coverage_entry_t g_coverage_table[COVERAGE_MAX_ENTRIES];
int g_coverage_count = 0;

/* strcmp is not in the kernel's freestanding string.h; implement locally */
static int str_eq(const char* a, const char* b) {
    size_t la = strlen(a);
    return la == strlen(b) && memcmp(a, b, la) == 0;
}

/* =============================================================================
 * Helper Functions - Reduce complexity of coverage_print_summary
 * =============================================================================
 */

/**
 * Convert integer to string and print via serial
 * Handles negative numbers and zero
 * @param value Value to print
 */
static void print_number(int value) {
    char buf[16];
    int idx = 0;

    // Handle negative numbers
    bool negative = (value < 0);

    /* MED-003 FIX: negating INT_MIN (-2147483648) directly is UB in signed
     * arithmetic.  Convert via int64_t so the negation is well-defined, then
     * store in uint32_t for the digit-extraction loop. */
    uint32_t uval = negative ?
        static_cast<uint32_t>(-static_cast<int64_t>(value)) :
        static_cast<uint32_t>(value);

    // Convert to string
    if (uval == 0) {
        buf[idx++] = '0';
    } else {
        char temp[16];
        int temp_idx = 0;
        while (uval > 0) {
            temp[temp_idx++] = static_cast<char>('0' + static_cast<int>(uval % 10));
            uval /= 10;
        }
        // Reverse
        while (temp_idx > 0) {
            buf[idx++] = temp[--temp_idx];
        }
    }

    // Add negative sign if needed
    if (negative) {
        // Shift right and add '-'
        for (int i = idx; i > 0; i--) {
            buf[i] = buf[i - 1];
        }
        buf[0] = '-';
        idx++;
    }

    buf[idx] = '\0';
    serial_write_str(buf);
}

/**
 * Print coverage table header
 */
static void print_table_header(void) {
    serial_write_str("Function                          | Line | Hits\r\n");
    serial_write_str("----------------------------------|------|------\r\n");
}

/**
 * Print single coverage table entry
 * @param entry Pointer to coverage entry
 */
static void print_table_entry(const coverage_entry_t* entry) {
    // Function name (truncated to 32 chars, padded)
    const char* fn = entry->function;
    char fn_buf[33];
    int j;

    // Copy up to 32 chars
    for (j = 0; j < 32 && fn[j] != '\0'; j++) {
        fn_buf[j] = fn[j];
    }
    // Pad with spaces
    while (j < 32) {
        fn_buf[j++] = ' ';
    }
    fn_buf[32] = '\0';

    serial_write_str(fn_buf);
    serial_write_str(" | ");

    // Line number (padded to 6 chars)
    print_number(entry->line);
    serial_write_str(" | ");

    // Hit count
    print_number(entry->hits);
    serial_write_str("\r\n");
}

/**
 * Print coverage table (all entries)
 */
static void print_coverage_table(void) {
    for (int i = 0; i < g_coverage_count; i++) {
        print_table_entry(&g_coverage_table[i]);
    }
}

/**
 * Print labeled value (e.g., "Total Functions:   42")
 * @param label Label text
 * @param value Value to print
 */
static void print_labeled_value(const char* label, int value) {
    serial_write_str(label);
    print_number(value);
    serial_write_str("\r\n");
}

/**
 * Print summary section with statistics
 */
static void print_summary(void) {
    serial_write_str("\r\n");
    serial_write_str("========================================\r\n");
    serial_write_str("Summary\r\n");
    serial_write_str("========================================\r\n");

    // Function statistics
    print_labeled_value("Total Functions:   ", coverage_get_total_functions());
    print_labeled_value("Hit Functions:     ", coverage_get_hit_functions());
    print_labeled_value("Total Lines:       ", coverage_get_total_lines());
    print_labeled_value("Hit Lines:         ", coverage_get_hit_lines());

    // Calculate and print percentage
    serial_write_str("Line Coverage:     ");
    int total = coverage_get_total_lines();
    int hit = coverage_get_hit_lines();
    int pct = (total > 0) ? (hit * 100 / total) : 0;
    print_number(pct);
    serial_write_str("%\r\n");

    serial_write_str("========================================\r\n");
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

// Initialize coverage tracking
void coverage_init(void) {
    // Zero out the coverage table
    // Note: This is called after BSS is zeroed, but we explicitly
    // clear in case coverage_init is called multiple times
    if (g_coverage_count > 0) {
        // Reset hit counts (preserve function names)
        for (int i = 0; i < g_coverage_count; i++) {
            g_coverage_table[i].hits = 0;
        }
    }
}

// Record a coverage hit
void coverage_hit(const char* function, int line) {
    // Search for existing entry
    for (int i = 0; i < g_coverage_count; i++) {
        /* MED-002 FIX: compare string contents, not pointer addresses.
         * Checking line first (cheap int compare) short-circuits most misses
         * before the strcmp call. */
        if (g_coverage_table[i].line == line &&
            str_eq(g_coverage_table[i].function, function)) {
            g_coverage_table[i].hits++;
            return;
        }
    }

    // Add new entry if space available
    if (g_coverage_count < COVERAGE_MAX_ENTRIES) {
        g_coverage_table[g_coverage_count].function = function;
        g_coverage_table[g_coverage_count].line = line;
        g_coverage_table[g_coverage_count].hits = 1;
        g_coverage_count++;
    }
}

// Get total tracked functions
int coverage_get_total_functions(void) {
    int count = 0;
    const char* last_fn = nullptr;

    for (int i = 0; i < g_coverage_count; i++) {
        if (g_coverage_table[i].function != last_fn) {
            count++;
            last_fn = g_coverage_table[i].function;
        }
    }

    return count;
}

// Get number of functions with at least one hit
int coverage_get_hit_functions(void) {
    int count = 0;
    const char* last_fn = nullptr;
    bool last_fn_hit = false;

    for (int i = 0; i < g_coverage_count; i++) {
        if (g_coverage_table[i].function != last_fn) {
            // New function - check if previous had hits
            if (last_fn != nullptr && last_fn_hit) {
                count++;
            }
            last_fn = g_coverage_table[i].function;
            last_fn_hit = (g_coverage_table[i].hits > 0);
        } else if (g_coverage_table[i].hits > 0) {
            last_fn_hit = true;
        }
    }

    // Check last function
    if (last_fn != nullptr && last_fn_hit) {
        count++;
    }

    return count;
}

// Get total tracked lines
int coverage_get_total_lines(void) {
    return g_coverage_count;
}

// Get number of lines with at least one hit
int coverage_get_hit_lines(void) {
    int count = 0;
    for (int i = 0; i < g_coverage_count; i++) {
        if (g_coverage_table[i].hits > 0) {
            count++;
        }
    }
    return count;
}

// Print coverage summary via serial
// Refactored to reduce cognitive complexity (was 57, now < 10)
void coverage_print_summary(void) {
    // Header
    serial_write_str("\r\n");
    serial_write_str("========================================\r\n");
    serial_write_str("GLOBEX_OS Code Coverage Report\r\n");
    serial_write_str("========================================\r\n");
    serial_write_str("\r\n");

    // Table header and entries
    print_table_header();
    print_coverage_table();

    // Summary section
    print_summary();
}
