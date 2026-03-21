#ifndef COVERAGE_H
#define COVERAGE_H

/*
 * GLOBEX_OS Manual Code Coverage Instrumentation
 * ===============================================
 *
 * This header provides manual code coverage tracking for the freestanding
 * kernel. Since gcov/libgcov are not available without a standard library,
 * we use a simple function/line tracking mechanism.
 *
 * Usage:
 * ------
 * 1. Add COVERAGE_TRACK(fn, line) at the start of functions you want to track
 * 2. Build with COVERAGE_ENABLE=1
 * 3. Run the kernel - coverage data is output via serial
 * 4. Analyze coverage_report.txt
 *
 * Example:
 * --------
 * void my_function(void) {
 *     COVERAGE_TRACK(my_function, __LINE__);
 *     // ... function body ...
 * }
 *
 * Or use the macro at function entry:
 * COVERAGE_FN(my_function);
 *
 * Coverage Data Format (serial output):
 * -------------------------------------
 * [COV] function_name:line hit_count
 */

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of tracked functions/lines
#define COVERAGE_MAX_ENTRIES 512

// Coverage entry structure
typedef struct {
    const char* function;  // Function name
    int line;              // Line number
    int hits;              // Number of times executed
} coverage_entry_t;

// Global coverage table (defined in coverage.cpp)
extern coverage_entry_t g_coverage_table[COVERAGE_MAX_ENTRIES];
extern int g_coverage_count;

// Initialize coverage tracking (called at kernel startup)
void coverage_init(void);

// Record a coverage hit
void coverage_hit(const char* function, int line);

// Print coverage summary via serial
void coverage_print_summary(void);

// Get coverage statistics
int coverage_get_total_functions(void);
int coverage_get_hit_functions(void);
int coverage_get_total_lines(void);
int coverage_get_hit_lines(void);

/*
 * Coverage Tracking Macros
 * ========================
 */

#ifdef COVERAGE_ENABLE

// Track a function entry (call at function start)
#define COVERAGE_TRACK(fn, line) coverage_hit(#fn, line)

// Shorthand: track function at its definition line
#define COVERAGE_FN(fn) coverage_hit(#fn, __LINE__)

// Track a specific line (for branch coverage)
#define COVERAGE_LINE(line) coverage_hit(__func__, line)

// Track a conditional branch
#define COVERAGE_BRANCH(cond)                                                                      \
    do {                                                                                           \
        if (cond) {                                                                                \
            coverage_hit(__func__, __LINE__);                                                      \
        }                                                                                          \
    } while (0)

// Track both branches of a conditional
#define COVERAGE_IF(cond)                                                                          \
    if (cond) {                                                                                    \
        coverage_hit(__func__, __LINE__);

#define COVERAGE_ELSE()                                                                            \
    }                                                                                              \
    else {                                                                                         \
        coverage_hit(__func__, __LINE__);                                                          \
    }

#else

// No-op when coverage is disabled
#define COVERAGE_TRACK(fn, line) ((void) 0)
#define COVERAGE_FN(fn) ((void) 0)
#define COVERAGE_LINE(line) ((void) 0)
#define COVERAGE_BRANCH(cond) ((void) 0)
#define COVERAGE_IF(cond) if (cond)
#define COVERAGE_ELSE() else

#endif  // COVERAGE_ENABLE

#ifdef __cplusplus
}
#endif

#endif  // COVERAGE_H
