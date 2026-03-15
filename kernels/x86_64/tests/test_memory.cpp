#include "test_memory.h"
#include "serial.h"
#include "panic.h"
#include "constants.h"

/* ==========================================
 * Memory Mapping Test
 * ==========================================
 * Verifies page table setup and high memory access.
 * Tests 2GiB identity mapping and CR3 validation.
 * ========================================== */

// Forward declaration from main.cpp
// Naming convention: test_<purpose> for consistency
extern volatile uint32_t* test_high_mem_ptr;

void test_memory_mapping() {
    serial_write_str("\r\n=== Memory Mapping Test ===\r\n");
    serial_write_str("Page tables: 2GiB mapped (0x00000000-0x7FFFFFFF)\r\n");
    serial_write_str("Testing access to 80MB (0x05000000)...\r\n");

    // ==========================================
    // Page Table Verification [HIGH-006]
    // ==========================================
    // Read CR3 to verify page table base address
    uint64_t cr3_value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3_value));

    // Mask off PCID bits (lower 12 bits) to get page table base
    uint64_t pt_base = cr3_value & 0xFFFFFFFFF000ULL;

    serial_write_str("CR3 value: 0x");
    serial_write_hex64(cr3_value);
    serial_write_str("\r\n");
    serial_write_str("Page table base: 0x");
    serial_write_hex64(pt_base);
    serial_write_str("\r\n");

    // Verify CR3 points to a valid address (should be in .boot.data region)
    // .boot.data starts after kernel, typically around 1MB-2MB range
    if (pt_base > 0x100000 && pt_base < 0x10000000) {
        serial_write_str("CR3 validation: OK\r\n");
    } else {
        serial_write_str("CR3 validation: FAILED\r\n");
        panic("Page table CR3 validation failed", static_cast<uint32_t>(cr3_value));
    }

    // Verify identity mapping by reading L4 entry
    // L4 is at pt_base, entry 0 should point to L3 table
    const volatile uint64_t* l4_entry = reinterpret_cast<const volatile uint64_t*>(pt_base);
    uint64_t l3_addr = l4_entry[0] & 0xFFFFFFFFF000ULL;

    serial_write_str("L4[0] -> L3 at: 0x");
    serial_write_hex64(l3_addr);
    serial_write_str("\r\n");

    // L3 should be 4KB aligned and right after L4
    if ((l3_addr & 0xFFF) == 0 && l3_addr == pt_base + 0x1000) {
        serial_write_str("L4->L3 mapping: OK\r\n");
    } else {
        serial_write_str("L4->L3 mapping: FAILED\r\n");
        panic("Page table L4->L3 mapping failed", static_cast<uint32_t>(l3_addr));
    }

    // Test read/write in high memory
    uint32_t test_pattern = 0xDEADBEEF;
    uint32_t read_back = 0;

    // Write pattern to high memory
    *test_high_mem_ptr = test_pattern;

    // Read back
    read_back = *test_high_mem_ptr;

    serial_write_str("Write pattern: 0x");
    serial_write_hex(test_pattern);
    serial_write_str("\r\n");
    serial_write_str("Read back:   0x");
    serial_write_hex(read_back);
    serial_write_str("\r\n");

    if (read_back == test_pattern) {
        serial_write_str("[MEM TEST] PASSED: Memory access at 80MB works\r\n");
    } else {
        serial_write_str("[MEM TEST] FAILED: Memory access at 80MB failed!\r\n");
        panic("Memory mapping test failed", read_back);
    }
}
