#ifndef GDT_H
#define GDT_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type-Safe Wrappers (NewType Pattern)
 * =============================================================================
 * These wrappers prevent accidentally swapping parameters of similar types.
 * Use explicit constructors to create wrapped values.
 *
 * Example:
 *   gdt_set_entry(entry, gdt_base(0x0), gdt_limit(0xFFFFFFFF),
 *                 gdt_access(GDT_ACCESS_CODE), gdt_granularity(0xCF));
 * =============================================================================
 */

/**
 * @brief Wrapper for GDT base address
 * @note Prevents swapping with limit or access parameters
 */
typedef struct {
    uint64_t value;
} gdt_base_t;

/**
 * @brief Wrapper for GDT limit value
 * @note Prevents swapping with base or access parameters
 */
typedef struct {
    uint64_t value;
} gdt_limit_t;

/**
 * @brief Wrapper for GDT access byte
 * @note Prevents swapping with base/limit/granularity
 */
typedef struct {
    uint8_t value;
} gdt_access_t;

/**
 * @brief Wrapper for GDT granularity byte
 * @note Prevents swapping with other parameters
 */
typedef struct {
    uint8_t value;
} gdt_granularity_t;

/**
 * @brief Helper to create gdt_base_t
 */
static inline gdt_base_t gdt_base(uint64_t v) {
    gdt_base_t w = {v};
    return w;
}

/**
 * @brief Helper to create gdt_limit_t
 */
static inline gdt_limit_t gdt_limit(uint64_t v) {
    gdt_limit_t w = {v};
    return w;
}

/**
 * @brief Helper to create gdt_access_t
 */
static inline gdt_access_t gdt_access(uint8_t v) {
    gdt_access_t w = {v};
    return w;
}

/**
 * @brief Helper to create gdt_granularity_t
 */
static inline gdt_granularity_t gdt_granularity(uint8_t v) {
    gdt_granularity_t w = {v};
    return w;
}

/* =============================================================================
 * GDT - Global Descriptor Table
 * =============================================================================
 *
 * The GDT defines the characteristics of the memory segments used in
 * protected mode and long mode. In 64-bit mode, most segments are
 * compatibility segments with base=0, limit=0xFFFFFFFF.
 *
 * Segments implemented:
 *   - Null descriptor (required by spec)
 *   - Kernel code segment (execute-only, readable)
 *   - Kernel data segment (read/write)
 *   - User code segment (execute-only, readable)
 *   - User data segment (read/write)
 *   - TSS (Task State Segment) for IST and RSP switching
 *
 * Memory Layout (flat memory model):
 *   All segments: base = 0x00000000, limit = 0xFFFFFFFF
 *   This provides a flat 4GB address space.
 *
 * =============================================================================
 */

/* =============================================================================
 * GDT Entry Structure (Segment Descriptor)
 * =============================================================================
 * Standard x86_64 segment descriptor format:
 *
 * Bits 0-15:   Limit 0-15
 * Bits 16-31:  Base 0-15
 * Bits 32-39:  Base 16-23
 * Bits 40-43:  Type (S=1) or Reserved (S=0)
 * Bits 44:     S (Storage segment: 1=code/data, 0=system)
 * Bits 45-46:  DPL (Descriptor Privilege Level): 0=kernel, 3=user
 * Bits 47:     P (Present)
 * Bits 48-51:  Limit 16-19
 * Bits 52:     AVL (Available for software)
 * Bits 53:     L (64-bit mode active)
 * Bits 54:     D/B (Default operation size / Default stack pointer size)
 * Bits 55:     G (Granularity: 1=4KB pages, 0=1 byte)
 * Bits 56-63:  Base 24-31
 * Bits 64-95:  Base 32-63 (only for TSS and call gates)
 *
 * HIGH-005 FIX: TSS descriptors require 16 bytes (two GDT entries) in x86_64.
 * The upper 32 bits of the base address are stored in the second entry.
 * =============================================================================
 */

#pragma pack(push, 1)
typedef struct {
    uint16_t limit_low;  /* Limit bits 0-15 */
    uint16_t base_low;   /* Base bits 0-15 */
    uint8_t base_middle; /* Base bits 16-23 */
    uint8_t access;      /* Access byte (type, S, DPL, P) */
    uint8_t granularity; /* Limit 16-19 + flags */
    uint8_t base_high;   /* Base bits 24-31 */
} gdt_entry_t;

/**
 * HIGH-005 FIX: TSS High Descriptor (upper 32 bits of base)
 *
 * In x86_64, TSS descriptors are 16 bytes total:
 * - First 8 bytes: Standard descriptor (gdt_entry_t)
 * - Second 8 bytes: Upper 32 bits of base address
 */
typedef struct {
    uint32_t base_high32; /* Base bits 32-63 */
    uint32_t reserved;    /* Must be zero */
} tss_descriptor_high_t;
#pragma pack(pop)

/* =============================================================================
 * GDT Pointer Structure (for LGDT instruction)
 * =============================================================================
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t limit; /* Size of GDT - 1 */
    uint64_t base;  /* Linear address of GDT */
} gdt_pointer_t;
#pragma pack(pop)

/* =============================================================================
 * TSS (Task State Segment) Structure
 * =============================================================================
 * Used for privilege level transitions and interrupt stack switching.
 * Only the RSP0 field is used in our implementation for kernel stack.
 * =============================================================================
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t reserved1;  /* Reserved */
    uint64_t rsp0;       /* Ring 0 Stack Pointer */
    uint64_t rsp1;       /* Ring 1 Stack Pointer (unused) */
    uint64_t rsp2;       /* Ring 2 Stack Pointer (unused) */
    uint64_t ist1;       /* Interrupt Stack Table 1 (HIGH-005: dedicated #DF stack) */
    uint64_t ist2;       /* Interrupt Stack Table 2 (unused) */
    uint64_t ist3;       /* Interrupt Stack Table 3 (unused) */
    uint64_t ist4;       /* Interrupt Stack Table 4 (unused) */
    uint64_t ist5;       /* Interrupt Stack Table 5 (unused) */
    uint64_t ist6;       /* Interrupt Stack Table 6 (unused) */
    uint64_t ist7;       /* Interrupt Stack Table 7 (unused) */
    uint64_t reserved2;  /* Reserved */
    uint16_t reserved3;  /* Reserved */
    uint16_t iomap_base; /* I/O Map Base (disabled) */
} tss_t;
#pragma pack(pop)

/* =============================================================================
 * Access Byte Constants
 * =============================================================================
 * Format: 0b1000PDPL
 *   Bit 7 (P): Present (must be 1 for valid segment)
 *   Bit 6-5 (DPL): Descriptor Privilege Level (0=kernel, 3=user)
 *   Bit 4 (S): Storage segment (1=code/data, 0=system)
 *   Bit 0-3 (Type): Segment type
 * =============================================================================
 */

/* Access byte for code segment */
#define GDT_ACCESS_CODE_PRESENT 0x98  /* 1001 1000: P=1, DPL=0, S=1, Type=8 (execute-only) */
#define GDT_ACCESS_CODE_READABLE 0x9A /* 1001 1010: P=1, DPL=0, S=1, Type=10 (execute+read) */

/* Access byte for data segment */
#define GDT_ACCESS_DATA_PRESENT 0x90   /* 1001 0000: P=1, DPL=0, S=1, Type=0 (data) */
#define GDT_ACCESS_DATA_READWRITE 0x92 /* 1001 0010: P=1, DPL=0, S=1, Type=2 (read/write) */

/* Access byte for user segments */
#define GDT_ACCESS_USER_CODE 0xF8 /* 1111 1000: P=1, DPL=3, S=1, Type=8 */
#define GDT_ACCESS_USER_DATA 0xF2 /* 1111 0010: P=1, DPL=3, S=1, Type=2 */

/* Access byte for TSS (system segment) */
#define GDT_ACCESS_TSS 0x89 /* 1000 1001: P=1, DPL=0, S=0, Type=9 (TSS available) */

/* =============================================================================
 * Granularity Byte Constants
 * =============================================================================
 * Format: 0bGLDB
 *   Bit 7 (G): Granularity (1=4KB, 0=1 byte)
 *   Bit 6 (L): 64-bit mode (1=active for code segment)
 *   Bit 5 (D/B): Default size (0 for 64-bit, 1 for 32-bit)
 *   Bit 4 (AVL): Available for software
 *   Bit 0-3: Limit 16-19
 * =============================================================================
 */

/* Granularity for 64-bit kernel code segment */
#define GDT_GRANULARITY_64BIT_CODE 0xAF /* 1010 1111: G=1, L=1, D=0, limit=0xF */

/* Granularity for data segments */
#define GDT_GRANULARITY_DATA 0x00 /* 0000 0000: G=0, L=0, D=0, limit=0x0 */

/* =============================================================================
 * GDT Configuration
 * =============================================================================
 */

/*
 * HIGH-005 FIX: Increased to 7 entries to accommodate 16-byte TSS descriptor.
 * TSS requires two consecutive GDT entries in x86_64.
 */
#define GDT_ENTRIES 7

/* GDT entry indices */
#define GDT_INDEX_NULL 0
#define GDT_INDEX_KERNEL_CODE 1
#define GDT_INDEX_KERNEL_DATA 2
#define GDT_INDEX_USER_CODE 3
#define GDT_INDEX_USER_DATA 4
#define GDT_INDEX_TSS 5
/* Entry 6 is reserved for TSS high descriptor (internal use) */

/* Segment selectors (index << 3 | RPL) */
#define GDT_SELECTOR_KERNEL_CODE ((GDT_INDEX_KERNEL_CODE << 3) | 0) /* 0x08 */
#define GDT_SELECTOR_KERNEL_DATA ((GDT_INDEX_KERNEL_DATA << 3) | 0) /* 0x10 */
#define GDT_SELECTOR_USER_CODE ((GDT_INDEX_USER_CODE << 3) | 3)     /* 0x1B */
#define GDT_SELECTOR_USER_DATA ((GDT_INDEX_USER_DATA << 3) | 3)     /* 0x23 */
#define GDT_SELECTOR_TSS ((GDT_INDEX_TSS << 3) | 0)                 /* 0x28 */

/* =============================================================================
 * GDT API
 * =============================================================================
 */

/**
 * @brief Initialize the Global Descriptor Table
 *
 * Sets up GDT with the following segments:
 *   - Null descriptor
 *   - Kernel code segment (64-bit, DPL=0)
 *   - Kernel data segment (DPL=0)
 *   - User code segment (64-bit, DPL=3)
 *   - User data segment (DPL=3)
 *   - TSS descriptor
 *
 * @note Must be called before enabling interrupts
 * @note Loads the GDT using LGDT instruction
 */
void gdt_init(void);

/**
 * @brief Initialize the Task State Segment
 *
 * Sets up TSS with kernel stack pointer for privilege transitions.
 *
 * @param kernel_stack Pointer to kernel stack to use for interrupts
 *
 * @note Must be called after gdt_init()
 * @note TSS is loaded using LTR instruction
 */
void tss_init(uint64_t kernel_stack);

/**
 * @brief Get pointer to GDT table
 * @return Pointer to GDT entry array
 *
 * @note For internal use by assembly boot code
 */
gdt_entry_t* gdt_get_table(void);

/**
 * @brief Get GDT pointer structure
 * @return Pointer to GDT pointer (for LGDT)
 *
 * @note For internal use by assembly boot code
 */
gdt_pointer_t* gdt_get_pointer(void);

/**
 * @brief Get pointer to TSS structure
 * @return Pointer to TSS
 *
 * @note For setting up kernel stack
 */
tss_t* tss_get(void);

/**
 * @brief Check IST stack canaries for overflow detection
 * @return 0 if all canaries are valid, bitmask of corrupted stacks otherwise
 *
 * CRIT-KERN-002 FIX: Stack overflow detection for IST stacks.
 * Checks the canary value at the top of each IST stack:
 *   - Bit 0: IST1 (#DF double-fault) stack
 *   - Bit 1: IST2 (NMI) stack
 *   - Bit 2: IST3 (#MC machine-check) stack
 *
 * @note Safe to call from interrupt handlers
 * @note Returns immediately if canary corruption detected
 *
 * @example
 *   @code
 *   if (check_ist_stack_canaries()) {
 *       LOG_PANIC("IST stack overflow detected - system compromised");
 *   }
 *   @endcode
 */
int check_ist_stack_canaries(void);

#ifdef __cplusplus
}
#endif

#endif /* GDT_H */
