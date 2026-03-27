/* =============================================================================
 * Stack Guard Page - HIGH-005 fix
 * =============================================================================
 *
 * The kernel boot stack sits in .boot.data, directly above the boot page
 * tables.  Without a guard page, a stack overflow silently overwrites
 * page_table_l2_1 / page_table_l2_0, causing mysterious faults later.
 *
 * This module splits the 2MiB huge page that contains the guard region
 * into 4KiB entries and marks the guard page not-present.  The first
 * stack overflow then triggers a clean #PF instead of silent corruption.
 *
 * Combined with IST1 on the #DF handler (also part of HIGH-005), even a
 * stack overflow severe enough to make the #PF handler recurse will be
 * caught by the double-fault handler running on its own safe stack.
 * =============================================================================
 */

#include "guard.h"
#include "constants.h"
#include "serial.h"
#include "stddef.h"

/* -------------------------------------------------------------------------
 * Symbols exported from main.asm
 * ------------------------------------------------------------------------- */

/** Address of the guard page (4KiB, placed immediately below stack_bottom) */
extern "C" char stack_guard;

/**
 * First 1GiB L2 page table (512 × 8-byte entries, covers 0x00000000–0x3FFFFFFF).
 * Each entry maps a 2MiB huge page.  We modify the entry that covers the
 * guard page to point at a 4KiB-granularity L1 table instead.
 */
extern "C" uint64_t page_table_l2_0[512];

/* -------------------------------------------------------------------------
 * L1 page table for the guard region
 * -------------------------------------------------------------------------
 * Exactly one 4KiB-aligned page; replaces a single 2MiB L2 entry.
 * Stored in .bss so it is zero-initialised before stack_guard_init() runs.
 * ------------------------------------------------------------------------- */
static uint64_t g_guard_l1_table[512] __attribute__((aligned(4096)));

/* -------------------------------------------------------------------------
 * stack_guard_init
 * ------------------------------------------------------------------------- */
void stack_guard_init(void) {
    uint64_t guard_addr = (uint64_t) &stack_guard;

    /* 2MiB-aligned base of the huge page that contains the guard page */
    uint64_t huge_base = guard_addr & ~(uint64_t) (0x1FFFFF);

    /* HIGH-NEW-002: IDENTITY-MAPPING ASSUMPTION — This code writes physical
     * addresses directly into the L1 page table entries
     * (huge_base + i * PAGE_SIZE).  This is correct only because the boot
     * code identity-maps the first 2GiB (virtual == physical,
     * 0x00000000–0x7FFFFFFF) via huge-page L2 entries in main.asm.
     *
     * If the kernel is ever moved to a higher-half mapping or KASLR is
     * added, every address written into g_guard_l1_table must be replaced
     * with a virt_to_phys() translation, and the L2 entry pointer must
     * likewise use the physical address of g_guard_l1_table. */

    /* Index of that 2MiB entry inside page_table_l2_0
     * (valid because the kernel is always in the first 1GiB) */
    size_t l2_idx = (huge_base >> 21) & 0x1FF;

    /* LOW-NEW-008 FIX: Reject an out-of-range l2_idx before using it as
     * an array index into page_table_l2_0[512].  Under the current layout
     * the guard page is always within the first 1GiB so this index is
     * always < 512; the check guards against future linker script changes
     * that move the stack above 1GiB. */
    if (l2_idx >= 512) {
        serial_write_str("[GUARD] FATAL: guard address outside L2 table range\r\n");
        return;
    }

    /* Index of the guard page inside the new 4KiB L1 table */
    size_t guard_l1_idx = (guard_addr >> 12) & 0x1FF;

    /* Fill L1: map every 4KiB page in this 2MiB region as present + writable */
    for (size_t i = 0; i < 512; i++) {
        g_guard_l1_table[i] = (huge_base + (uint64_t) i * PAGE_SIZE) | 0x3;
    }

    /* Mark the guard page not-present (entry = 0) */
    g_guard_l1_table[guard_l1_idx] = 0;

    /* Replace the 2MiB huge-page L2 entry with a pointer to the L1 table.
     * Bit 7 (PS/huge) is NOT set, so the CPU treats it as a normal L2 entry
     * pointing to a 4KiB page table. */
    page_table_l2_0[l2_idx] = (uint64_t) g_guard_l1_table | 0x3;

    /* Flush the TLB entry for the guard page */
    __asm__ volatile("invlpg (%0)" ::"r"(guard_addr) : "memory");

    serial_write_str("[GUARD] Stack guard page at 0x");
    serial_write_hex64(guard_addr);
    serial_write_str(" (not-present)\r\n");
}
