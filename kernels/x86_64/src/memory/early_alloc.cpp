#include "early_alloc.h"
#include "serial.h"
#include "print.h"
#include "string.h"
#include "barriers.h"

/* =============================================================================
 * Early Allocator State
 * =============================================================================
 * All variables in .bss (no explicit initializers)
 */

static uint8_t* g_early_pool = 0;       /* Pool base address */
static size_t g_early_size = 0;          /* Total pool size */
static size_t g_early_used = 0;          /* Bytes allocated */
static int g_early_initialized = 0;      /* 1 if initialized */

/* Default BSS pool (1MB) */
static uint8_t g_early_alloc_pool[EARLY_ALLOC_POOL_SIZE] __attribute__((section(".bss")));

/* =============================================================================
 * Initialization
 * =============================================================================
 */

int early_alloc_init(void* pool, size_t size) {
    if (!pool || size == 0) {
        return 0;
    }

    g_early_pool = (uint8_t*)pool;
    g_early_size = size;
    g_early_used = 0;
    mb();
    g_early_initialized = 1;
    mb();

    return 1;
}

int early_alloc_init_default(void) {
    return early_alloc_init(g_early_alloc_pool, EARLY_ALLOC_POOL_SIZE);
}

int early_alloc_is_initialized(void) {
    rmb();
    return g_early_initialized;
}

/* =============================================================================
 * Allocation Functions
 * =============================================================================
 */

void* early_alloc(size_t size) {
    return early_alloc_align(size, EARLY_ALLOC_ALIGNMENT);
}

void* early_alloc_align(size_t size, size_t alignment) {
    if (!g_early_initialized) {
        return 0;
    }

    if (size == 0) {
        size = 1;  /* Allocate at least 1 byte */
    }

    /* Ensure alignment is power of 2 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        alignment = EARLY_ALLOC_ALIGNMENT;
    }

    /* HIGH-007 FIX: Check for size overflow first */
    if (size > g_early_size) {
        serial_write_str("[EARLY_ALLOC] HIGH-007: Request too large: ");
        serial_write_dec(size);
        serial_write_str(" > ");
        serial_write_dec(g_early_size);
        serial_write_str("\r\n");
        return 0;
    }

    /* Calculate aligned offset */
    uintptr_t current = (uintptr_t)(g_early_pool + g_early_used);
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    /* HIGH-007 FIX: Safe overflow check for addition
     * a + b > c  =>  a > c - b  (when c >= b)
     */
    if (padding > g_early_size - g_early_used ||
        size > g_early_size - g_early_used - padding) {
        /* Out of memory */
        serial_write_str("[EARLY_ALLOC] Out of memory! Requested: ");
        serial_write_dec(size);
        serial_write_str(" bytes\r\n");
        return 0;
    }

    /* Allocate */
    size_t total_needed = padding + size;
    g_early_used += total_needed;
    wmb();

    void* ptr = (void*)aligned;

    /* Zero the allocated memory */
    memset(ptr, 0, size);

    return ptr;
}

size_t early_alloc_get_used(void) {
    rmb();
    return g_early_used;
}

size_t early_alloc_get_free(void) {
    rmb();
    return g_early_size - g_early_used;
}

size_t early_alloc_get_pool_size(void) {
    return g_early_size;
}

/* =============================================================================
 * Debug Functions
 * =============================================================================
 */

void early_alloc_print_stats(void) {
    serial_write_str("\r\n=== Early Allocator Statistics ===\r\n");

    serial_write_str("Pool size:     ");
    serial_write_dec(g_early_size);
    serial_write_str(" bytes\r\n");

    serial_write_str("Used:          ");
    serial_write_dec(g_early_used);
    serial_write_str(" bytes\r\n");

    serial_write_str("Free:          ");
    serial_write_dec(g_early_size - g_early_used);
    serial_write_str(" bytes\r\n");

    if (g_early_size > 0) {
        uint32_t pct = (g_early_used * 100) / g_early_size;
        serial_write_str("Utilization:   ");
        serial_write_dec(pct);
        serial_write_str("%\r\n");
    }

    serial_write_str("=================================\r\n");
}
