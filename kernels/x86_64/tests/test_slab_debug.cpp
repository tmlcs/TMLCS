#include "test_slab_debug.h"
#include "slab.h"
#include "heap.h"
#include "bitmap.h"
#include "serial.h"
#include "print.h"
#include "string.h"
#include "spinlock.h"

/* =============================================================================
 * Slab Allocator Debug Tests
 * =============================================================================
 * These tests diagnose the serial corruption issue in slab allocator.
 * 
 * Investigation approach:
 * 1. Check memory layout and boundaries
 * 2. Verify slab pool integrity
 * 3. Test allocation patterns that trigger corruption
 * 4. Capture serial output byte-by-byte
 * =============================================================================
 */

/* Debug: Print memory region as hex dump */
static void debug_hex_dump(const char* label, const void* addr, size_t len) {
    const uint8_t* p = (const uint8_t*)addr;
    
    serial_write_str("\r\n");
    serial_write_str(label);
    serial_write_str(" @ 0x");
    serial_write_hex64((uint64_t)addr);
    serial_write_str(" (");
    serial_write_dec(len);
    serial_write_str(" bytes)\r\n");
    
    for (size_t i = 0; i < len && i < 256; i += 16) {
        /* Print offset */
        if (i < 0x10) serial_write_str("0");
        if (i < 0x100) serial_write_str("0");
        if (i < 0x1000) serial_write_str("0");
        serial_write_hex((uint32_t)i);
        serial_write_str(": ");
        
        /* Print hex bytes */
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t b = p[i + j];
            if (b < 0x10) serial_write_char('0');
            serial_write_hex((uint32_t)b);
            serial_write_char(' ');
        }
        
        serial_write_str("  ");
        
        /* Print ASCII */
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t b = p[i + j];
            if (b >= 0x20 && b < 0x7F) {
                serial_write_char((char)b);
            } else {
                serial_write_char('.');
            }
        }
        
        serial_write_str("\r\n");
    }
}

/* External reference to slab pool (allocated from early_alloc) */
extern uint8_t* g_slab_pool;
extern size_t g_slab_pool_size;

/* =============================================================================
 * Test 1: Verify Slab Pool Memory Region
 * =============================================================================
 */
static void test_slab_pool_integrity(void) {
    serial_write_str("\r\n=== Test: Slab Pool Integrity ===\r\n");
    
    /* Get slab state */
    slab_state_t* state = slab_get_state();
    
    serial_write_str("Slab initialized: ");
    serial_write_dec(state->initialized);
    serial_write_str("\r\n");
    
    serial_write_str("Total allocs: ");
    serial_write_dec(state->total_allocations);
    serial_write_str("\r\n");
    
    serial_write_str("Total frees: ");
    serial_write_dec(state->total_frees);
    serial_write_str("\r\n");
    
    serial_write_str("Total slabs: ");
    serial_write_dec(state->total_slabs);
    serial_write_str("\r\n");
    
    /* Dump first 256 bytes of slab pool */
    if (g_slab_pool) {
        debug_hex_dump("Slab Pool (first 256B)", g_slab_pool, 256);
    } else {
        serial_write_str("Slab pool not allocated yet\r\n");
    }
    
    serial_write_str("[OK] Pool integrity check complete\r\n");
}

/* =============================================================================
 * Test 2: Single Allocation Debug
 * =============================================================================
 */
static void test_single_alloc_debug(void) {
    serial_write_str("\r\n=== Test: Single Allocation Debug ===\r\n");
    
    /* Allocate one 32-byte object */
    void* ptr = kmem_alloc(32);
    
    serial_write_str("Allocation result: 0x");
    serial_write_hex64((uint64_t)ptr);
    serial_write_str("\r\n");
    
    if (ptr != NULL) {
        /* Write pattern to memory */
        uint8_t* bytes = (uint8_t*)ptr;
        for (int i = 0; i < 32; i++) {
            bytes[i] = (uint8_t)(i & 0xFF);
        }
        
        serial_write_str("Pattern written, verifying...\r\n");
        
        /* Verify pattern */
        int ok = 1;
        for (int i = 0; i < 32 && ok; i++) {
            if (bytes[i] != (uint8_t)(i & 0xFF)) {
                ok = 0;
                serial_write_str("Corruption at offset ");
                serial_write_dec(i);
                serial_write_str(": expected ");
                serial_write_dec(i & 0xFF);
                serial_write_str(", got ");
                serial_write_dec(bytes[i]);
                serial_write_str("\r\n");
            }
        }
        
        if (ok) {
            serial_write_str("[OK] Memory pattern verified\r\n");
        }
        
        /* Dump allocated memory */
        debug_hex_dump("Allocated Memory", ptr, 32);
        
        /* Free */
        kmem_free(ptr, 32);
        serial_write_str("Memory freed\r\n");
    } else {
        serial_write_str("[FAIL] Allocation returned NULL\r\n");
    }
}

/* =============================================================================
 * Test 3: Multiple Allocations Debug
 * =============================================================================
 */
static void test_multiple_allocs_debug(void) {
    serial_write_str("\r\n=== Test: Multiple Allocations Debug ===\r\n");
    
    #define NUM_ALLOCS 10
    void* ptrs[NUM_ALLOCS];
    
    /* Allocate */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = kmem_alloc(32);
        serial_write_str("Alloc ");
        serial_write_dec(i);
        serial_write_str(": 0x");
        serial_write_hex64((uint64_t)ptrs[i]);
        serial_write_str("\r\n");
    }
    
    /* Fill with unique patterns */
    for (int i = 0; i < NUM_ALLOCS && ptrs[i] != NULL; i++) {
        uint8_t* bytes = (uint8_t*)ptrs[i];
        for (int j = 0; j < 32; j++) {
            bytes[j] = (uint8_t)((i * 16 + j) & 0xFF);
        }
    }
    
    serial_write_str("Patterns written, verifying...\r\n");
    
    /* Verify */
    int errors = 0;
    for (int i = 0; i < NUM_ALLOCS && ptrs[i] != NULL; i++) {
        uint8_t* bytes = (uint8_t*)ptrs[i];
        for (int j = 0; j < 32; j++) {
            uint8_t expected = (uint8_t)((i * 16 + j) & 0xFF);
            if (bytes[j] != expected) {
                errors++;
                if (errors < 5) {
                    serial_write_str("Block ");
                    serial_write_dec(i);
                    serial_write_str(" offset ");
                    serial_write_dec(j);
                    serial_write_str(": exp ");
                    serial_write_dec(expected);
                    serial_write_str(" got ");
                    serial_write_dec(bytes[j]);
                    serial_write_str("\r\n");
                }
            }
        }
    }
    
    if (errors == 0) {
        serial_write_str("[OK] All patterns verified\r\n");
    } else {
        serial_write_str("[FAIL] ");
        serial_write_dec(errors);
        serial_write_str(" corruption errors\r\n");
    }
    
    /* Free all */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            kmem_free(ptrs[i], 32);
        }
    }
    
    serial_write_str("All freed\r\n");
    
    #undef NUM_ALLOCS
}

/* =============================================================================
 * Test 4: Serial Output Timing Test
 * =============================================================================
 */
static void test_serial_timing(void) {
    serial_write_str("\r\n=== Test: Serial Timing Debug ===\r\n");
    
    /* Test rapid serial output */
    serial_write_str("Test rapid output: ");
    for (int i = 0; i < 20; i++) {
        serial_write_dec(i);
        serial_write_char(' ');
    }
    serial_write_str("\r\n");
    
    /* Test with delays */
    serial_write_str("Test with delays: ");
    for (int i = 0; i < 20; i++) {
        serial_write_dec(i);
        serial_write_char(' ');
        
        /* Small delay */
        for (volatile int j = 0; j < 100; j++) {
            __asm__ volatile("nop");
        }
    }
    serial_write_str("\r\n");
    
    serial_write_str("[OK] Serial timing test complete\r\n");
}

/* =============================================================================
 * Test 5: Stack Usage Analysis
 * =============================================================================
 */
static void test_stack_analysis(void) {
    serial_write_str("\r\n=== Test: Stack Analysis ===\r\n");
    
    /* Check current stack pointer */
    uint64_t rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    
    serial_write_str("Current RSP: 0x");
    serial_write_hex64(rsp);
    serial_write_str("\r\n");
    
    /* Allocate some memory and check if it affects stack */
    void* ptr = kmem_alloc(32);
    
    uint64_t rsp2;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp2));
    
    serial_write_str("RSP after alloc: 0x");
    serial_write_hex64(rsp2);
    serial_write_str("\r\n");
    
    serial_write_str("Stack delta: ");
    serial_write_dec((uint32_t)(rsp - rsp2));
    serial_write_str(" bytes\r\n");
    
    if (ptr != NULL) {
        serial_write_str("Allocated ptr: 0x");
        serial_write_hex64((uint64_t)ptr);
        serial_write_str("\r\n");
        serial_write_str("Ptr distance from stack: ");
        serial_write_dec((uint32_t)((uint64_t)ptr - rsp2));
        serial_write_str(" bytes\r\n");
        
        kmem_free(ptr, 32);
    }
    
    serial_write_str("[OK] Stack analysis complete\r\n");
}

/* =============================================================================
 * Test 6: Slab State Before/After Stress
 * =============================================================================
 */
static void test_slab_stress_debug(void) {
    serial_write_str("\r\n=== Test: Slab Stress Debug ===\r\n");
    
    /* Get initial state */
    slab_state_t* state_before = slab_get_state();
    serial_write_str("Before stress:\r\n");
    serial_write_str("  Allocations: ");
    serial_write_dec(state_before->total_allocations);
    serial_write_str("\r\n");
    serial_write_str("  Slabs: ");
    serial_write_dec(state_before->total_slabs);
    serial_write_str("\r\n");
    
    /* Allocate many objects */
    #define STRESS_COUNT 20
    void* ptrs[STRESS_COUNT];
    int allocated = 0;
    
    serial_write_str("\r\nAllocating ");
    serial_write_dec(STRESS_COUNT);
    serial_write_str(" objects...\r\n");
    
    for (int i = 0; i < STRESS_COUNT; i++) {
        ptrs[i] = kmem_alloc(32);
        if (ptrs[i] != NULL) {
            allocated++;
            
            /* Fill with data */
            uint8_t* bytes = (uint8_t*)ptrs[i];
            for (int j = 0; j < 32; j++) {
                bytes[j] = (uint8_t)((i + j) & 0xFF);
            }
        }
    }
    
    serial_write_str("Allocated: ");
    serial_write_dec(allocated);
    serial_write_str("/");
    serial_write_dec(STRESS_COUNT);
    serial_write_str("\r\n");
    
    /* Get state after */
    slab_state_t* state_after = slab_get_state();
    serial_write_str("\r\nAfter stress:\r\n");
    serial_write_str("  Allocations: ");
    serial_write_dec(state_after->total_allocations);
    serial_write_str("\r\n");
    serial_write_str("  Slabs: ");
    serial_write_dec(state_after->total_slabs);
    serial_write_str("\r\n");
    
    /* Verify memory integrity */
    serial_write_str("\r\nVerifying memory...\r\n");
    int errors = 0;
    for (int i = 0; i < STRESS_COUNT && ptrs[i] != NULL; i++) {
        uint8_t* bytes = (uint8_t*)ptrs[i];
        for (int j = 0; j < 32; j++) {
            uint8_t expected = (uint8_t)((i + j) & 0xFF);
            if (bytes[j] != expected) {
                errors++;
            }
        }
    }
    
    if (errors == 0) {
        serial_write_str("[OK] Memory integrity verified\r\n");
    } else {
        serial_write_str("[FAIL] ");
        serial_write_dec(errors);
        serial_write_str(" corruption errors\r\n");
    }
    
    /* Free all */
    serial_write_str("Freeing...\r\n");
    for (int i = 0; i < STRESS_COUNT; i++) {
        if (ptrs[i] != NULL) {
            kmem_free(ptrs[i], 32);
        }
    }
    
    serial_write_str("[OK] Stress debug complete\r\n");
    
    #undef STRESS_COUNT
}

/* =============================================================================
 * Main Debug Test Runner
 * =============================================================================
 */
void test_slab_debug_all(void) {
    serial_write_str("\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("GLOBEX_OS Slab Allocator DEBUG Tests\r\n");
    serial_write_str("============================================\r\n");
    
    /* Ensure slab is initialized */
    if (!slab_is_initialized()) {
        serial_write_str("[INFO] Initializing slab...\r\n");
        if (!slab_init()) {
            serial_write_str("[FAIL] slab_init() failed!\r\n");
            return;
        }
    }
    
    /* Run debug tests */
    test_slab_pool_integrity();
    test_single_alloc_debug();
    test_multiple_allocs_debug();
    test_serial_timing();
    test_stack_analysis();
    test_slab_stress_debug();
    
    serial_write_str("\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("DEBUG Tests Complete\r\n");
    serial_write_str("============================================\r\n");
}
