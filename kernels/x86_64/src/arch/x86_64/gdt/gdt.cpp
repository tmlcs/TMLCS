/* =============================================================================
 * GDT - Global Descriptor Table Implementation
 * =============================================================================
 *
 * This module implements the Global Descriptor Table for x86_64 long mode.
 * While segments are mostly unused in 64-bit mode (flat memory model),
 * the GDT is still required for:
 *   - TSS (Task State Segment) for privilege transitions
 *   - User/kernel mode separation (DPL)
 *   - Compatibility with protected mode code
 *
 * =============================================================================
 */

#include "gdt.h"
#include "stddef.h"

/* =============================================================================
 * GDT Table - Static storage
 * =============================================================================
 * Aligned to 16 bytes for LGDT instruction efficiency.
 * 
 * HIGH-005 FIX: Increased to 7 entries to accommodate 16-byte TSS descriptor.
 * TSS requires two GDT entries (16 bytes total) in x86_64.
 * =============================================================================
 */
static gdt_entry_t gdt_table[GDT_ENTRIES] __attribute__((aligned(16)));

/* =============================================================================
 * TSS High Descriptor - Static storage
 * =============================================================================
 * HIGH-005 FIX: Upper 32 bits of TSS base address.
 * In x86_64, TSS descriptors are 16 bytes (two GDT entries).
 * The second entry contains base[32:63] and reserved bits.
 * =============================================================================
 */
static tss_descriptor_high_t g_tss_high __attribute__((aligned(8)));

/* =============================================================================
 * GDT Pointer - Static storage
 * =============================================================================
 * Aligned to 16 bytes for LGDT instruction efficiency.
 * =============================================================================
 */
static gdt_pointer_t gdt_pointer;

/* =============================================================================
 * TSS - Task State Segment
 * =============================================================================
 * Aligned to 16 bytes for proper access.
 * =============================================================================
 */
static tss_t tss_entry __attribute__((aligned(16)));

/* =============================================================================
 * External assembly functions
 * =============================================================================
 */
extern "C" {
void gdt_load(gdt_pointer_t* gdtp);
void tss_load(uint16_t tss_selector);
}

/* =============================================================================
 * GDT Entry Helper Function
 * =============================================================================
 * Creates a GDT entry with the specified parameters.
 *
 * @param entry Pointer to GDT entry to initialize
 * @param base Base address of segment (wrapped in gdt_base_t for type safety)
 * @param limit Limit (size - 1) of segment (wrapped in gdt_limit_t for type safety)
 * @param access Access byte (wrapped in gdt_access_t for type safety)
 * @param granularity Granularity byte (wrapped in gdt_granularity_t for type safety)
 *
 * @note Using type-safe wrappers prevents accidentally swapping parameters
 */
static void gdt_set_entry(gdt_entry_t* entry, gdt_base_t base, gdt_limit_t limit,
                          gdt_access_t access, gdt_granularity_t granularity) {
    entry->limit_low = limit.value & 0xFFFF;
    entry->base_low = base.value & 0xFFFF;
    entry->base_middle = (base.value >> 16) & 0xFF;
    entry->access = access.value;
    entry->granularity = granularity.value | ((limit.value >> 16) & 0x0F);
    entry->base_high = (base.value >> 24) & 0xFF;
}

/* =============================================================================
 * TSS Entry Helper Function
 * =============================================================================
 * Creates a TSS descriptor entry in the GDT.
 *
 * HIGH-005 FIX: Properly sets up 16-byte TSS descriptor for x86_64.
 * The TSS descriptor requires two GDT entries:
 *   - Entry 1 (gdt_entry_t): Standard descriptor with base[0:31]
 *   - Entry 2 (tss_descriptor_high_t): Upper 32 bits of base address
 *
 * @param entry Pointer to GDT entry to initialize
 * @param base Base address of TSS (wrapped in gdt_base_t for type safety)
 * @param limit Limit of TSS (sizeof(tss_t) - 1) (wrapped in gdt_limit_t for type safety)
 *
 * @note Using type-safe wrappers prevents accidentally swapping base/limit
 * @note TSS descriptor is 16 bytes total in x86_64
 * =============================================================================
 */
static void gdt_set_tss_entry(gdt_entry_t* entry, gdt_base_t base, gdt_limit_t limit) {
    /* TSS descriptor format differs from standard segment descriptors */
    entry->limit_low = limit.value & 0xFFFF;
    entry->base_low = base.value & 0xFFFF;
    entry->base_middle = (base.value >> 16) & 0xFF;
    entry->access = GDT_ACCESS_TSS; /* Type 9: 32-bit TSS available */
    entry->granularity = (limit.value >> 16) & 0x0F;
    entry->base_high = (base.value >> 24) & 0xFF;

    /* HIGH-005 FIX: Set upper 32 bits of base address (required for x86_64) */
    /* This is stored in the second 8-byte entry following the TSS descriptor */
    g_tss_high.base_high32 = (uint32_t)(base.value >> 32);
    g_tss_high.reserved = 0;  /* Must be zero per Intel spec */
}

/* =============================================================================
 * gdt_init - Initialize Global Descriptor Table
 * =============================================================================
 */
void gdt_init(void) {
    /* Clear GDT table */
    for (size_t i = 0; i < GDT_ENTRIES; i++) {
        gdt_table[i].limit_low = 0;
        gdt_table[i].base_low = 0;
        gdt_table[i].base_middle = 0;
        gdt_table[i].access = 0;
        gdt_table[i].granularity = 0;
        gdt_table[i].base_high = 0;
    }

    /* Entry 0: Null descriptor (required by Intel spec) */
    gdt_table[GDT_INDEX_NULL].limit_low = 0;
    gdt_table[GDT_INDEX_NULL].base_low = 0;
    gdt_table[GDT_INDEX_NULL].base_middle = 0;
    gdt_table[GDT_INDEX_NULL].access = 0;
    gdt_table[GDT_INDEX_NULL].granularity = 0;
    gdt_table[GDT_INDEX_NULL].base_high = 0;

    /* Entry 1: Kernel code segment
     * Base: 0x00000000, Limit: 0xFFFFFFFF
     * Type: Execute-only + Readable, DPL: 0 (kernel)
     * 64-bit mode: L=1, D=0
     */
    gdt_set_entry(&gdt_table[GDT_INDEX_KERNEL_CODE], gdt_base(0x00000000),
                  gdt_limit(0xFFFFFFFF), gdt_access(GDT_ACCESS_CODE_READABLE),
                  gdt_granularity(GDT_GRANULARITY_64BIT_CODE));

    /* Entry 2: Kernel data segment
     * Base: 0x00000000, Limit: 0xFFFFFFFF
     * Type: Read/Write, DPL: 0 (kernel)
     */
    gdt_set_entry(&gdt_table[GDT_INDEX_KERNEL_DATA], gdt_base(0x00000000),
                  gdt_limit(0xFFFFFFFF), gdt_access(GDT_ACCESS_DATA_READWRITE),
                  gdt_granularity(GDT_GRANULARITY_DATA));

    /* Entry 3: User code segment
     * Base: 0x00000000, Limit: 0xFFFFFFFF
     * Type: Execute-only + Readable, DPL: 3 (user)
     * 64-bit mode: L=1, D=0
     */
    gdt_set_entry(&gdt_table[GDT_INDEX_USER_CODE], gdt_base(0x00000000),
                  gdt_limit(0xFFFFFFFF), gdt_access(GDT_ACCESS_USER_CODE),
                  gdt_granularity(GDT_GRANULARITY_64BIT_CODE));

    /* Entry 4: User data segment
     * Base: 0x00000000, Limit: 0xFFFFFFFF
     * Type: Read/Write, DPL: 3 (user)
     */
    gdt_set_entry(&gdt_table[GDT_INDEX_USER_DATA], gdt_base(0x00000000),
                  gdt_limit(0xFFFFFFFF), gdt_access(GDT_ACCESS_USER_DATA),
                  gdt_granularity(GDT_GRANULARITY_DATA));

    /* Entry 5: TSS descriptor (initialized by tss_init) */
    /* Will be set up when tss_init() is called */

    /* HIGH-005 FIX: Clear TSS high descriptor */
    g_tss_high.base_high32 = 0;
    g_tss_high.reserved = 0;

    /* Set up GDT pointer */
    gdt_pointer.limit = sizeof(gdt_table) - 1;
    gdt_pointer.base = (uint64_t) gdt_table;

    /* Load GDT using LGDT instruction */
    gdt_load(&gdt_pointer);
}

/* =============================================================================
 * tss_init - Initialize Task State Segment
 * =============================================================================
 * HIGH-005 FIX: Properly initializes 16-byte TSS descriptor for x86_64.
 * After setting up the standard descriptor, copies the high descriptor
 * to the next GDT entry.
 * =============================================================================
 */
void tss_init(uint64_t kernel_stack) {
    /* Clear TSS entry */
    tss_entry.reserved1 = 0;
    tss_entry.rsp0 = kernel_stack;
    tss_entry.rsp1 = 0;
    tss_entry.rsp2 = 0;
    tss_entry.ist1 = 0;
    tss_entry.ist2 = 0;
    tss_entry.ist3 = 0;
    tss_entry.ist4 = 0;
    tss_entry.ist5 = 0;
    tss_entry.ist6 = 0;
    tss_entry.ist7 = 0;
    tss_entry.reserved2 = 0;
    tss_entry.reserved3 = 0;
    tss_entry.iomap_base = 0; /* No I/O permission bitmap */

    /* Set up TSS descriptor in GDT */
    gdt_set_tss_entry(&gdt_table[GDT_INDEX_TSS], gdt_base((uint64_t) &tss_entry),
                      gdt_limit(sizeof(tss_entry) - 1));

    /* HIGH-005 FIX: Copy TSS high descriptor to next GDT entry
     * In x86_64, TSS descriptor is 16 bytes (two consecutive GDT entries)
     * The high descriptor contains base[32:63] in the first 4 bytes
     * 
     * Second GDT entry layout (Intel SDM Vol 3A, Fig 3-8):
     * - Bytes 0-1: Base[32:47] (stored in limit_low field)
     * - Bytes 2-3: Base[48:63] (stored in base_low field)
     * - Bytes 4-7: Reserved (zero)
     */
    uint32_t base_upper = g_tss_high.base_high32;  /* Bits 32-63 of TSS base */
    
    /* Copy upper 32 bits of base to second GDT entry */
    gdt_table[GDT_INDEX_TSS + 1].limit_low = base_upper & 0xFFFF;         /* Base[32:47] */
    gdt_table[GDT_INDEX_TSS + 1].base_low = (base_upper >> 16) & 0xFFFF;  /* Base[48:63] */
    gdt_table[GDT_INDEX_TSS + 1].base_middle = 0;  /* Reserved */
    gdt_table[GDT_INDEX_TSS + 1].access = 0;       /* Reserved */
    gdt_table[GDT_INDEX_TSS + 1].granularity = 0;  /* Reserved */
    gdt_table[GDT_INDEX_TSS + 1].base_high = 0;    /* Reserved */

    /* Load TSS using LTR instruction */
    tss_load(GDT_SELECTOR_TSS);
}

/* =============================================================================
 * gdt_get_table - Get pointer to GDT table
 * =============================================================================
 */
gdt_entry_t* gdt_get_table(void) {
    return gdt_table;
}

/* =============================================================================
 * gdt_get_pointer - Get pointer to GDT pointer structure
 * =============================================================================
 */
gdt_pointer_t* gdt_get_pointer(void) {
    return &gdt_pointer;
}

/* =============================================================================
 * tss_get - Get pointer to TSS structure
 * =============================================================================
 */
tss_t* tss_get(void) {
    return &tss_entry;
}
