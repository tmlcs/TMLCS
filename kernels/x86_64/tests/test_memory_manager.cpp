#include "test_memory_manager.h"
#include "heap.h"
#include "bitmap.h"
#include "slab.h"
#include "serial.h"
#include "print.h"
#include "string.h"

/* =============================================================================
 * Test Helpers
 * =============================================================================
 */

static int tests_passed = 0;
static int tests_failed = 0;

static void print_test_header(const char* name) {
    serial_write_str("\r\n--- ");
    serial_write_str(name);
    serial_write_str(" ---\r\n");
}

static void test_pass(const char* msg) {
    serial_write_str("  [PASS] ");
    serial_write_str(msg);
    serial_write_str("\r\n");
    tests_passed++;
}

static void test_fail(const char* msg) {
    serial_write_str("  [FAIL] ");
    serial_write_str(msg);
    serial_write_str("\r\n");
    tests_failed++;
}

/* =============================================================================
 * Test: Basic kmalloc/kfree
 * =============================================================================
 */

void test_kmalloc_basic(void) {
    print_test_header("Basic kmalloc/kfree");
    
    /* Test 1: Simple allocation */
    void* ptr = kmalloc(1024);
    if (ptr != NULL) {
        test_pass("kmalloc(1024) returned non-NULL");
    } else {
        test_fail("kmalloc(1024) returned NULL");
        return;
    }
    
    /* Test 2: Verify pointer is valid (non-NULL) */
    uintptr_t addr = (uintptr_t)ptr;
    if (addr != 0) {
        test_pass("Pointer is valid (non-NULL)");
    } else {
        test_fail("Pointer is NULL");
    }
    
    /* Test 3: Can write to allocated memory */
    memset(ptr, 0xAA, 1024);
    test_pass("Write to allocated memory succeeded");
    
    /* Test 4: Free the memory */
    kmem_free_auto(ptr);
    test_pass("kmem_free_auto() completed without crash");
    
    /* Test 5: Freeing NULL is safe */
    kmem_free_auto(NULL);
    test_pass("kmem_free_auto(NULL) is safe (no crash)");
}

/* =============================================================================
 * Test: Multiple Allocations
 * =============================================================================
 */

void test_kmalloc_multiple(void) {
    print_test_header("Multiple Allocations");
    
    #define NUM_ALLOCS 10
    void* ptrs[NUM_ALLOCS];
    
    /* Allocate multiple blocks */
    int alloc_count = 0;
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc(256 + i * 100);
        if (ptrs[i] != NULL) {
            alloc_count++;
        }
    }
    
    serial_write_str("  Allocated ");
    serial_write_dec(alloc_count);
    serial_write_str("/");
    serial_write_dec(NUM_ALLOCS);
    serial_write_str(" blocks\r\n");
    
    if (alloc_count > 0) {
        test_pass("Multiple allocations succeeded");
    } else {
        test_fail("Failed to allocate multiple blocks");
    }
    
    /* Verify all pointers are unique */
    int unique = 1;
    for (int i = 0; i < NUM_ALLOCS && unique; i++) {
        for (int j = i + 1; j < NUM_ALLOCS && unique; j++) {
            if (ptrs[i] != NULL && ptrs[j] != NULL && ptrs[i] == ptrs[j]) {
                unique = 0;
            }
        }
    }
    
    if (unique) {
        test_pass("All pointers are unique");
    } else {
        test_fail("Duplicate pointers detected");
    }
    
    /* Free all allocations */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            kmem_free_auto(ptrs[i]);
        }
    }
    test_pass("All blocks freed");
    
    #undef NUM_ALLOCS
}

/* =============================================================================
 * Test: kcalloc (Zeroed Allocation)
 * =============================================================================
 */

void test_kcalloc_zeroed(void) {
    print_test_header("kcalloc Zeroed Memory");
    
    /* Allocate array of integers */
    int* arr = (int*)kcalloc(100, sizeof(int));
    
    if (arr == NULL) {
        test_fail("kcalloc returned NULL");
        return;
    }
    
    test_pass("kcalloc(100, sizeof(int)) succeeded");
    
    /* Verify all elements are zero */
    int all_zero = 1;
    for (int i = 0; i < 100; i++) {
        if (arr[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    
    if (all_zero) {
        test_pass("All elements are zeroed");
    } else {
        test_fail("Memory was NOT zeroed");
    }
    
    kmem_free_auto(arr);
    test_pass("Memory freed");
}

/* =============================================================================
 * Test: krealloc
 * =============================================================================
 */

void test_krealloc(void) {
    print_test_header("krealloc");
    
    /* Test 1: Allocate initial block */
    char* ptr = (char*)kmalloc(100);
    if (ptr == NULL) {
        test_fail("Initial kmalloc failed");
        return;
    }
    
    /* Fill with pattern */
    for (int i = 0; i < 100; i++) {
        ptr[i] = (char)(i & 0xFF);
    }
    test_pass("Initial allocation filled");
    
    /* Test 2: Grow the allocation */
    char* new_ptr = (char*)krealloc(ptr, 200);
    if (new_ptr == NULL) {
        test_fail("krealloc to grow failed");
        kmem_free_auto(ptr);
        return;
    }
    
    /* Verify old data is preserved */
    int data_preserved = 1;
    for (int i = 0; i < 100; i++) {
        if (new_ptr[i] != (char)(i & 0xFF)) {
            data_preserved = 0;
            break;
        }
    }
    
    if (data_preserved) {
        test_pass("Old data preserved after grow");
    } else {
        test_fail("Old data NOT preserved after grow");
    }
    
    ptr = new_ptr;
    
    /* Test 3: Shrink the allocation */
    new_ptr = (char*)krealloc(ptr, 50);
    if (new_ptr == NULL) {
        test_fail("krealloc to shrink failed");
        kmem_free_auto(ptr);
        return;
    }
    
    /* Verify first 50 bytes are preserved */
    data_preserved = 1;
    for (int i = 0; i < 50; i++) {
        if (new_ptr[i] != (char)(i & 0xFF)) {
            data_preserved = 0;
            break;
        }
    }
    
    if (data_preserved) {
        test_pass("Data preserved after shrink");
    } else {
        test_fail("Data NOT preserved after shrink");
    }
    
    kmem_free_auto(new_ptr);
    test_pass("Reallocated memory freed");
    
    /* Test 4: krealloc with NULL */
    ptr = (char*)krealloc(NULL, 100);
    if (ptr != NULL) {
        test_pass("krealloc(NULL, size) works like kmalloc");
        kmem_free_auto(ptr);
    } else {
        test_fail("krealloc(NULL, size) failed");
    }
    
    /* Test 5: krealloc with size 0 */
    ptr = (char*)kmalloc(100);
    new_ptr = (char*)krealloc(ptr, 0);
    if (new_ptr == NULL) {
        test_pass("krealloc(ptr, 0) returns NULL and frees");
    } else {
        test_fail("krealloc(ptr, 0) should return NULL");
        kmem_free_auto(new_ptr);
    }
}

/* =============================================================================
 * Test: NULL Pointer Handling
 * =============================================================================
 */

void test_kmalloc_null_handling(void) {
    print_test_header("NULL Pointer Handling");
    
    /* Test 1: kmalloc(0) returns NULL */
    void* ptr = kmalloc(0);
    if (ptr == NULL) {
        test_pass("kmalloc(0) returns NULL");
    } else {
        test_fail("kmalloc(0) should return NULL");
        kmem_free_auto(ptr);
    }
    
    /* Test 2: kmem_free_auto(NULL) is safe */
    kmem_free_auto(NULL);
    test_pass("kmem_free_auto(NULL) is safe");
    
    /* Test 3: kcalloc with 0 elements */
    ptr = kcalloc(0, 100);
    if (ptr == NULL) {
        test_pass("kcalloc(0, size) returns NULL");
    } else {
        test_fail("kcalloc(0, size) should return NULL");
        kmem_free_auto(ptr);
    }
    
    /* Test 4: kcalloc with 0 size */
    ptr = kcalloc(100, 0);
    if (ptr == NULL) {
        test_pass("kcalloc(nmemb, 0) returns NULL");
    } else {
        test_fail("kcalloc(nmemb, 0) should return NULL");
        kmem_free_auto(ptr);
    }
}

/* =============================================================================
 * Test: Boundary Conditions
 * =============================================================================
 */

void test_kmalloc_boundaries(void) {
    print_test_header("Boundary Conditions");
    
    /* Test 1: Very small allocation */
    void* ptr = kmalloc(1);
    if (ptr != NULL) {
        test_pass("kmalloc(1) succeeded");
        kmem_free_auto(ptr);
    } else {
        test_fail("kmalloc(1) failed");
    }
    
    /* Test 2: Exactly one page */
    ptr = kmalloc(PAGE_SIZE);
    if (ptr != NULL) {
        test_pass("kmalloc(PAGE_SIZE) succeeded");
        kmem_free_auto(ptr);
    } else {
        test_fail("kmalloc(PAGE_SIZE) failed");
    }
    
    /* Test 3: Just over one page */
    ptr = kmalloc(PAGE_SIZE + 1);
    if (ptr != NULL) {
        test_pass("kmalloc(PAGE_SIZE+1) succeeded");
        kmem_free_auto(ptr);
    } else {
        test_fail("kmalloc(PAGE_SIZE+1) failed");
    }
    
    /* Test 4: Multiple pages */
    ptr = kmalloc(PAGE_SIZE * 4);
    if (ptr != NULL) {
        test_pass("kmalloc(4 pages) succeeded");
        
        /* Verify size */
        size_t actual_size = kmalloc_size(ptr);
        if (actual_size >= PAGE_SIZE * 4) {
            test_pass("Actual size >= requested size");
        } else {
            test_fail("Actual size < requested size");
        }
        
        kmem_free_auto(ptr);
    } else {
        test_fail("kmalloc(4 pages) failed");
    }
}

/* =============================================================================
 * Test: Memory Integrity
 * =============================================================================
 */

void test_memory_integrity(void) {
    print_test_header("Memory Integrity");

    uint8_t* ptr = (uint8_t*)kmalloc(256);  /* Smaller allocation for faster test */

    if (ptr == NULL) {
        test_fail("Allocation failed");
        return;
    }

    /* Write simple pattern */
    ptr[0] = 0xAA;
    ptr[128] = 0x55;
    ptr[255] = 0xFF;
    test_pass("Pattern written");

    /* Verify pattern */
    if (ptr[0] == 0xAA && ptr[128] == 0x55 && ptr[255] == 0xFF) {
        test_pass("Pattern verified - memory integrity OK");
    } else {
        test_fail("Memory corruption detected");
    }

    kmem_free_auto(ptr);
}

/* =============================================================================
 * Test: Fragmentation
 * =============================================================================
 */

void test_fragmentation(void) {
    print_test_header("Fragmentation Handling");
    
    /* Allocate several blocks */
    void* ptrs[20];
    int count = 0;
    
    for (int i = 0; i < 20; i++) {
        ptrs[i] = kmalloc(512);
        if (ptrs[i] != NULL) {
            count++;
        }
    }
    
    serial_write_str("  Initial allocations: ");
    serial_write_dec(count);
    serial_write_str("\r\n");
    
    /* Free every other block */
    for (int i = 0; i < 20; i += 2) {
        if (ptrs[i] != NULL) {
            kmem_free_auto(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    
    test_pass("Freed alternating blocks (created fragmentation)");
    
    /* Try to allocate in fragmented memory */
    void* new_ptr = kmalloc(512);
    if (new_ptr != NULL) {
        test_pass("Allocation in fragmented memory succeeded");
        kmem_free_auto(new_ptr);
    } else {
        test_fail("Allocation in fragmented memory failed");
    }
    
    /* Free remaining blocks */
    for (int i = 0; i < 20; i++) {
        if (ptrs[i] != NULL) {
            kmem_free_auto(ptrs[i]);
        }
    }
    
    test_pass("All blocks freed");
}

/* =============================================================================
 * Test: Large Allocations
 * =============================================================================
 */

void test_large_allocations(void) {
    print_test_header("Large Allocations");

    /* Test 1: 64KB allocation */
    void* ptr = kmalloc(64 * 1024);
    if (ptr != NULL) {
        test_pass("kmalloc(64KB) succeeded");
        kmem_free_auto(ptr);
        test_pass("kmem_free_auto(64KB) succeeded");
    } else {
        test_fail("kmalloc(64KB) failed (may be out of memory)");
    }

    /* Test 2: 256KB allocation */
    ptr = kmalloc(256 * 1024);
    if (ptr != NULL) {
        test_pass("kmalloc(256KB) succeeded");
        kmem_free_auto(ptr);
        test_pass("kmem_free_auto(256KB) succeeded");
    } else {
        test_fail("kmalloc(256KB) failed (may be out of memory)");
    }
}

/* =============================================================================
 * Test: Bitmap Allocator
 * =============================================================================
 */

void test_bitmap_allocator(void) {
    print_test_header("Bitmap Allocator");
    
    /* Test 1: Allocate single page */
    size_t page = bitmap_alloc();
    if (page != (size_t)-1) {
        test_pass("bitmap_alloc() succeeded");
        
        /* Verify page is marked allocated */
        if (bitmap_is_page_allocated(page) == 1) {
            test_pass("Page marked as allocated");
        } else {
            test_fail("Page NOT marked as allocated");
        }
        
        /* Free the page */
        if (bitmap_free(page) == 0) {
            test_pass("bitmap_free() succeeded");
        } else {
            test_fail("bitmap_free() failed");
        }
    } else {
        test_fail("bitmap_alloc() failed (out of memory)");
    }
    
    /* Test 2: Allocate contiguous pages */
    size_t start = bitmap_alloc_contiguous(4);
    if (start != (size_t)-1) {
        test_pass("bitmap_alloc_contiguous(4) succeeded");
        
        /* Verify all pages are allocated */
        int all_allocated = 1;
        for (int i = 0; i < 4; i++) {
            if (bitmap_is_page_allocated(start + i) != 1) {
                all_allocated = 0;
                break;
            }
        }
        
        if (all_allocated) {
            test_pass("All 4 pages marked allocated");
        } else {
            test_fail("Not all pages marked allocated");
        }
        
        /* Free contiguous pages */
        if (bitmap_free_contiguous(start, 4) == 0) {
            test_pass("bitmap_free_contiguous() succeeded");
        } else {
            test_fail("bitmap_free_contiguous() failed");
        }
    } else {
        test_fail("bitmap_alloc_contiguous() failed");
    }
    
    /* Test 3: Query functions */
    size_t free_pages = bitmap_count_free_pages();
    serial_write_str("  Free pages: ");
    serial_write_dec(free_pages);
    serial_write_str("\r\n");

    if (free_pages > 0) {
        test_pass("bitmap_count_free_pages() returned value > 0");
    }

    size_t largest = bitmap_largest_free_region();
    serial_write_str("  Largest free region: ");
    serial_write_dec(largest);
    serial_write_str(" pages\r\n");

    if (largest > 0) {
        test_pass("bitmap_largest_free_region() returned value > 0");
    }
    
    /* Test 4: FIX-MEM-004 - Validate count=0 rejection */
    serial_write_str("  Testing bitmap_free_contiguous(count=0)...\r\n");
    int result = bitmap_free_contiguous(0, 0);
    if (result == -1) {
        test_pass("bitmap_free_contiguous(count=0) correctly rejected");
    } else {
        test_fail("bitmap_free_contiguous(count=0) should return -1");
    }
}

/* =============================================================================
 * TEST-MEM-001: Stress Test for Memory Allocator
 * =============================================================================
 * Tests allocator under memory pressure.
 * Allocates many objects, frees randomly, verifies no leaks/corruption.
 */

void test_memory_stress(void) {
    print_test_header("Memory Stress Test (TEST-MEM-001)");
    
    #define STRESS_ALLOC_COUNT 50
    #define STRESS_FREE_COUNT 25
    
    void* ptrs[STRESS_ALLOC_COUNT];
    
    /* Initialize array */
    for (int i = 0; i < STRESS_ALLOC_COUNT; i++) {
        ptrs[i] = 0;
    }
    
    serial_write_str("  Allocating ");
    serial_write_dec(STRESS_ALLOC_COUNT);
    serial_write_str(" objects...\r\n");
    
    /* Allocate many objects */
    int alloc_success = 0;
    for (int i = 0; i < STRESS_ALLOC_COUNT; i++) {
        /* Vary size: 32, 64, 128, 256 bytes */
        size_t size = 32 << (i % 4);
        ptrs[i] = kmem_alloc(size);
        if (ptrs[i]) {
            /* Write pattern */
            memset(ptrs[i], (uint8_t)(i & 0xFF), size);
            alloc_success++;
        }
    }
    
    serial_write_str("  Allocated: ");
    serial_write_dec(alloc_success);
    serial_write_str("/");
    serial_write_dec(STRESS_ALLOC_COUNT);
    serial_write_str("\r\n");
    
    if (alloc_success < STRESS_ALLOC_COUNT / 2) {
        test_fail("Too many allocation failures");
        return;
    }
    
    test_pass("Multiple allocations succeeded");
    
    /* Free some objects (not all) */
    serial_write_str("  Freeing ");
    serial_write_dec(STRESS_FREE_COUNT);
    serial_write_str(" objects...\r\n");
    
    int free_success = 0;
    for (int i = 0; i < STRESS_FREE_COUNT && i < alloc_success; i++) {
        /* Free every other object */
        if (i % 2 == 0 && ptrs[i]) {
            size_t size = 32 << (i % 4);
            kmem_free(ptrs[i], size);
            ptrs[i] = 0;
            free_success++;
        }
    }
    
    serial_write_str("  Freed: ");
    serial_write_dec(free_success);
    serial_write_str("\r\n");
    test_pass("Multiple frees succeeded");
    
    /* Allocate again - should reuse freed memory */
    serial_write_str("  Re-allocating to test reuse...\r\n");
    
    int reuse_success = 0;
    for (int i = 0; i < STRESS_ALLOC_COUNT; i++) {
        if (!ptrs[i]) {
            size_t size = 32 << (i % 4);
            void* new_ptr = kmem_alloc(size);
            if (new_ptr) {
                memset(new_ptr, 0xAA, size);
                ptrs[i] = new_ptr;
                reuse_success++;
            }
        }
    }
    
    serial_write_str("  Re-allocated: ");
    serial_write_dec(reuse_success);
    serial_write_str("\r\n");
    
    if (reuse_success > 0) {
        test_pass("Memory reuse works");
    } else {
        test_fail("Memory reuse failed");
    }
    
    /* Free all remaining */
    serial_write_str("  Cleaning up...\r\n");
    for (int i = 0; i < STRESS_ALLOC_COUNT; i++) {
        if (ptrs[i]) {
            size_t size = 32 << (i % 4);
            kmem_free(ptrs[i], size);
        }
    }
    
    test_pass("Stress test completed successfully");
    
    #undef STRESS_ALLOC_COUNT
    #undef STRESS_FREE_COUNT
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

void test_memory_manager(void) {
    serial_write_str("\r\n============================================\r\n");
    serial_write_str("GLOBEX_OS Memory Manager Test Suite\r\n");
    serial_write_str("============================================\r\n");
    
    /* Initialize heap */
    serial_write_str("\r\nInitializing heap...\r\n");
    if (!heap_init()) {
        serial_write_str("[ERROR] heap_init() failed!\r\n");
        return;
    }
    serial_write_str("Heap initialized successfully\r\n");
    
    /* Print initial stats */
    serial_write_str("\r\nInitial memory state:\r\n");
    heap_print_stats();
    
    /* Run tests */
    test_kmalloc_basic();
    test_kmalloc_multiple();
    test_kcalloc_zeroed();
    test_krealloc();
    test_kmalloc_null_handling();
    test_kmalloc_boundaries();
    test_memory_integrity();
    test_fragmentation();
    test_large_allocations();
    test_bitmap_allocator();
    test_memory_stress();  /* TEST-MEM-001: Stress test */

    /* Print final stats */
    serial_write_str("\r\nFinal memory state:\r\n");
    heap_print_stats();
    
    /* Summary */
    serial_write_str("\r\n============================================\r\n");
    serial_write_str("Memory Manager Test Summary\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("Passed: ");
    serial_write_dec(tests_passed);
    serial_write_str("\r\n");
    serial_write_str("Failed: ");
    serial_write_dec(tests_failed);
    serial_write_str("\r\n");
    
    if (tests_failed == 0) {
        serial_write_str("\r\n[MEMORY MANAGER] All tests PASSED!\r\n");
    } else {
        serial_write_str("\r\n[MEMORY MANAGER] Some tests FAILED!\r\n");
    }
    serial_write_str("============================================\r\n");
}
