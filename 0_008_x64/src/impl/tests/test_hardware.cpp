#include "test_hardware.h"
#include "serial.h"

/* ==========================================
 * Hardware Information Test
 * ==========================================
 * Tests CPUID instruction for hardware detection.
 * Retrieves vendor ID, family/model/stepping, and
 * verifies long mode (64-bit) support.
 * ========================================== */

void test_hardware_info() {
    serial_write_str("\r\n=== Hardware Information Test ===\r\n");

    // Direct CPUID test
    uint32_t test_eax, test_ebx, test_ecx, test_edx;

    // CPUID leaf 0: Vendor ID
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(0)
        : "memory"
    );

    serial_write_str("CPU Max Leaf: ");
    serial_write_hex(test_eax);
    serial_write_str("\r\n");

    // Vendor string
    serial_write_str("Vendor: ");
    const char* vendor = (const char*)&test_ebx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    vendor = (const char*)&test_edx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    vendor = (const char*)&test_ecx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    serial_write_str("\r\n");

    // CPUID leaf 1: Processor Info
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(1)
        : "memory"
    );

    uint32_t stepping = test_eax & 0xF;
    uint32_t model = (test_eax >> 4) & 0xF;
    uint32_t family = (test_eax >> 8) & 0xF;

    serial_write_str("Family: ");
    serial_write_dec(family);
    serial_write_str(", Model: ");
    serial_write_dec(model);
    serial_write_str(", Stepping: ");
    serial_write_dec(stepping);
    serial_write_str("\r\n");

    // CPUID leaf 0x80000000: Extended leaf check
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(0x80000000)
        : "memory"
    );

    uint32_t max_extended = test_eax;

    // CPUID leaf 0x80000001: Long mode check
    int has_long_mode = 0;
    if (max_extended >= 0x80000001) {
        __asm__ volatile (
            "cpuid"
            : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
            : "a"(0x80000001)
            : "memory"
        );
        has_long_mode = (test_edx >> 29) & 1;
    }

    serial_write_str("Long Mode (64-bit): ");
    if (has_long_mode) {
        serial_write_str("Supported\r\n");
    } else {
        serial_write_str("NOT Supported\r\n");
    }

    serial_write_str("[HARDWARE INFO] CPU detection complete\r\n");
}
