#ifndef IDT_H
#define IDT_H

#include "constants.h" /* Centralized IDT/PIC constants */
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * IDT - Interrupt Descriptor Table
 * =============================================================================
 *
 * The IDT defines the handlers for hardware interrupts and software
 * exceptions. Each entry is a gate descriptor that points to an
 * interrupt handler function.
 *
 * =============================================================================
 * INTERRUPT VECTOR TABLE (0-31)
 * =============================================================================
 *
 * Vectors 0-7: CPU Exceptions (no error code)
 * ┌─────────┬──────────────────────────────────┬─────────────────────────────┐
 * │ Vector  │ Name                             │ Description                 │
 * ├─────────┼──────────────────────────────────┼─────────────────────────────┤
 * │   0     │ Divide Error (#DE)               │ DIV/IDIV instruction error  │
 * │   1     │ Debug (#DB)                      │ Debug exception             │
 * │   2     │ Non-Maskable Interrupt (NMI)     │ Hardware NMI                │
 * │   3     │ Breakpoint (#BP)                 │ INT 3 instruction           │
 * │   4     │ Overflow (#OF)                   │ INTO instruction            │
 * │   5     │ Bound Range (#BR)                │ BOUND instruction           │
 * │   6     │ Invalid Opcode (#UD)             │ Undefined instruction       │
 * │   7     │ Device Not Available (#NM)       │ No x87 FPU available        │
 * └─────────┴──────────────────────────────────┴─────────────────────────────┘
 *
 * Vectors 8-14: CPU Exceptions (with error code)
 * ┌─────────┬──────────────────────────────────┬─────────────────────────────┐
 * │ Vector  │ Name                             │ Description                 │
 * ├─────────┼──────────────────────────────────┼─────────────────────────────┤
 * │   8     │ Double Fault (#DF)               │ Unhandled exception         │
 * │   9     │ Reserved (Intel/AMD)             │ Stub handler                │
 * │  10     │ Invalid TSS (#TS)                │ Invalid task state segment  │
 * │  11     │ Segment Not Present (#NP)        │ Segment not present         │
 * │  12     │ Stack Fault (#SS)                │ Stack segment fault         │
 * │  13     │ General Protection (#GP)         │ Protection violation        │
 * │  14     │ Page Fault (#PF)                 │ Page not present/permission │
 * └─────────┴──────────────────────────────────┴─────────────────────────────┘
 *
 * Vectors 15-19: Reserved and Other Exceptions
 * ┌─────────┬──────────────────────────────────┬─────────────────────────────┐
 * │ Vector  │ Name                             │ Description                 │
 * ├─────────┼──────────────────────────────────┼─────────────────────────────┤
 * │  15     │ Reserved (Intel/AMD)             │ Stub handler                │
 * │  16     │ x87 FPU Error (#MF)              │ Floating-point error        │
 * │  17     │ Alignment Check (#AC)            │ Misaligned access           │
 * │  18     │ Machine Check (#MC)              │ Hardware error              │
 * │  19     │ SIMD FPU (#XM)                   │ SSE/AVX floating-point      │
 * └─────────┴──────────────────────────────────┴─────────────────────────────┘
 *
 * Vectors 20-31: Reserved for Future CPU Extensions
 * ┌─────────┬──────────────────────────────────┬─────────────────────────────┐
 * │ Vector  │ Name                             │ Description                 │
 * ├─────────┼──────────────────────────────────┼─────────────────────────────┤
 * │  20-31  │ Reserved (future extension)      │ Stub handlers for each      │
 * └─────────┴──────────────────────────────────┴─────────────────────────────┘
 *
 * Reference: Intel SDM Volume 3A, Section 6.9
 * "Vectors 20 through 31 are reserved for future expansion."
 *
 * =============================================================================
 * HARDWARE IRQs (32-47, PIC remapped to 0x20-0x2F)
 * =============================================================================
 *
 * ┌─────────┬──────────┬──────────────────────────────────────────────────────┐
 * │ Vector  │ IRQ      │ Device                                               │
 * ├─────────┼──────────┼──────────────────────────────────────────────────────┤
 * │  0x20   │ IRQ0     │ Programmable Interval Timer (PIT)                    │
 * │  0x21   │ IRQ1     │ Keyboard                                             │
 * │  0x22   │ IRQ2     │ Cascade (Slave PIC)                                  │
 * │  0x23   │ IRQ3     │ Serial Port 2 (COM2)                                 │
 * │  0x24   │ IRQ4     │ Serial Port 1 (COM1)                                 │
 * │  0x25   │ IRQ5     │ Parallel Port 2 / Sound Card                         │
 * │  0x26   │ IRQ6     │ Floppy Disk Controller                               │
 * │  0x27   │ IRQ7     │ Parallel Port 1 / Sound Card                         │
 * │  0x28   │ IRQ8     │ Real-Time Clock (RTC)                                │
 * │  0x29   │ IRQ9     │ ACPI / Available                                     │
 * │  0x2A   │ IRQ10    │ Available / USB                                      │
 * │  0x2B   │ IRQ11    │ Available / USB                                      │
 * │  0x2C   │ IRQ12    │ PS/2 Mouse                                           │
 * │  0x2D   │ IRQ13    │ x87 FPU Coprocessor                                  │
 * │  0x2E   │ IRQ14    │ Primary ATA/IDE                                      │
 * │  0x2F   │ IRQ15    │ Secondary ATA/IDE                                    │
 * └─────────┴──────────┴──────────────────────────────────────────────────────┘
 *
 * All IDT/PIC constants are now defined in constants.h:
 *   - IDT_ENTRIES, IDT_VECTOR_*, IDT_OFFSET_*
 *   - IDT_INTERRUPT_GATE, IDT_TRAP_GATE
 *   - PIC1_COMMAND, PIC1_DATA, PIC2_COMMAND, PIC2_DATA
 *   - PIC1_OFFSET, PIC2_OFFSET, PIC_EOI
 *   - ICW1_*, ICW4_*, ICW3_*
 *   - RFLAGS_IF_BIT, EXCEPTION_MESSAGE_COUNT
 *   - EXCEPTION_DE, EXCEPTION_DB, ..., EXCEPTION_XM
 *   - IRQ_PIT, IRQ_KEYBOARD, ..., IRQ_ATA2
 *
 * This ensures a single source of truth for all interrupt constants.
 * =============================================================================
 */

/* =============================================================================
 * IDT Entry Structure (Gate Descriptor)
 * =============================================================================
 * Standard x86_64 interrupt gate format (16 bytes):
 *
 * Bits 0-15:   Offset 0-15 (handler address bits 0-15)
 * Bits 16-31:  Segment selector (GDT selector)
 * Bits 32-34:  IST (Interrupt Stack Table) index
 * Bits 35-39:  Reserved (must be 0)
 * Bits 40-43:  Type (0xE = 32-bit interrupt gate)
 * Bits 44:     Reserved (must be 0)
 * Bits 45-46:  DPL (Descriptor Privilege Level)
 * Bits 47:     P (Present)
 * Bits 48-63:  Offset 16-31 (handler address bits 16-31)
 * Bits 64-95:  Offset 32-63 (handler address bits 32-63)
 * Bits 96-127: Reserved (must be 0)
 * =============================================================================
 */

#pragma pack(push, 1)
typedef struct {
    uint16_t offset_low;    /* Offset bits 0-15 */
    uint16_t selector;      /* Segment selector (GDT) */
    uint8_t ist;            /* IST index (bits 0-2) + reserved */
    uint8_t type_attr;      /* Type and attributes */
    uint16_t offset_middle; /* Offset bits 16-31 */
    uint32_t offset_high;   /* Offset bits 32-63 */
    uint32_t reserved;      /* Reserved */
} idt_entry_t;
#pragma pack(pop)

/* =============================================================================
 * IDT Pointer Structure (for LIDT instruction)
 * =============================================================================
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t limit; /* Size of IDT - 1 */
    uint64_t base;  /* Linear address of IDT */
} idt_pointer_t;
#pragma pack(pop)

/* =============================================================================
 * Exception Vector Aliases
 * =============================================================================
 * These are provided for backward compatibility.
 * Primary definitions are in constants.h.
 * =============================================================================
 */

/* =============================================================================
 * Exception and IRQ Vector Aliases
 * =============================================================================
 * These are provided for backward compatibility.
 * Primary definitions are in constants.h:
 *   - EXCEPTION_DE, EXCEPTION_DB, ..., EXCEPTION_XM
 *   - IRQ_PIT, IRQ_KEYBOARD, ..., IRQ_ATA2
 * =============================================================================
 */

/* =============================================================================
 * Stack Frame Structure (pushed by CPU on interrupt)
 * =============================================================================
 */
#pragma pack(push, 1)
typedef struct {
    /* Pushed by CPU (automatic) */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    /* Pushed by our assembly stub */
    uint64_t int_num;  /* Interrupt number */
    uint64_t err_code; /* Error code (if applicable, else 0) */

    /* Pushed by CPU (automatic) */
    uint64_t rip;    /* Instruction pointer */
    uint64_t cs;     /* Code segment */
    uint64_t rflags; /* Flags register */
    uint64_t rsp;    /* Stack pointer */
    uint64_t ss;     /* Stack segment */
} interrupt_frame_t;
#pragma pack(pop)

/* =============================================================================
 * Type-Safe Parameter Wrappers - Prevents parameter swapping bugs
 * =============================================================================
 * Using distinct types for handler, type_attr, dpl, port, and data prevents
 * accidentally swapping them when calling functions. This is a common source
 * of bugs in low-level code.
 *
 * These are NewType wrappers - zero-cost at runtime, but provide compile-time
 * type safety. The compiler will reject swapped arguments.
 *
 * Usage example:
 *   idt_set_gate(0x20, handler_addr(0xFFFFFFFF80100000),
 *                type_attr(IDT_INTERRUPT_GATE), dpl(0));
 * =============================================================================
 */

/**
 * @brief Type-safe interrupt handler address (64-bit)
 * @note Distinct from uint64_t to prevent parameter swapping
 */
typedef struct {
    uint64_t value;
} handler_addr_t;

/**
 * @brief Type-safe gate type attribute
 * @note Distinct from uint8_t to prevent parameter swapping
 */
typedef struct {
    uint8_t value;
} type_attr_t;

/**
 * @brief Type-safe Descriptor Privilege Level (0-3)
 * @note Distinct from uint8_t to prevent parameter swapping
 */
typedef struct {
    uint8_t value;
} dpl_t;

/**
 * @brief Type-safe I/O port address (16-bit)
 * @note Distinct from uint16_t to prevent parameter swapping
 */
typedef struct {
    uint16_t value;
} io_port_t;

/**
 * @brief Type-safe I/O data byte (8-bit)
 * @note Distinct from uint8_t to prevent parameter swapping
 */
typedef struct {
    uint8_t value;
} io_data_t;

/* =============================================================================
 * Inline constructors for type-safe wrappers
 * =============================================================================
 */

/**
 * Create a handler address wrapper
 * @param value 64-bit handler address
 * @return handler_addr_t wrapper
 */
static inline handler_addr_t handler_addr(uint64_t value) {
    handler_addr_t h;
    h.value = value;
    return h;
}

/**
 * Create a type attribute wrapper
 * @param value Gate type (e.g., IDT_INTERRUPT_GATE)
 * @return type_attr_t wrapper
 */
static inline type_attr_t type_attr(uint8_t value) {
    type_attr_t t;
    t.value = value;
    return t;
}

/**
 * Create a DPL wrapper
 * @param value Privilege level (0-3)
 * @return dpl_t wrapper
 */
static inline dpl_t dpl(uint8_t value) {
    dpl_t d;
    d.value = value;
    return d;
}

/**
 * Create an I/O port wrapper
 * @param value Port address (e.g., 0x20, 0x21)
 * @return io_port_t wrapper
 */
static inline io_port_t io_port(uint16_t value) {
    io_port_t p;
    p.value = value;
    return p;
}

/**
 * Create an I/O data wrapper
 * @param value Data byte
 * @return io_data_t wrapper
 */
static inline io_data_t io_data(uint8_t value) {
    io_data_t d;
    d.value = value;
    return d;
}

/* =============================================================================
 * IDT API
 * =============================================================================
 */

/**
 * @brief Initialize the Interrupt Descriptor Table
 *
 * Sets up IDT with handlers for:
 *   - CPU exceptions (INT 0-31)
 *   - Hardware IRQs (INT 32-47, PIC remapped)
 *
 * @note Must be called after gdt_init()
 * @note Must be called before enabling interrupts (sti)
 */
void idt_init(void);

/**
 * @brief Register an interrupt handler
 *
 * Using type-safe wrappers prevents accidentally swapping handler/type_attr/dpl.
 *
 * @param vector Interrupt vector (0-255)
 * @param handler Handler address wrapper from handler_addr()
 * @param type_attr Gate type wrapper from type_attr()
 * @param dpl DPL wrapper from dpl()
 *
 * @example
 *   // Type-safe: compiler rejects swapped arguments
 *   idt_set_gate(0x20, handler_addr(handler), type_attr(IDT_INTERRUPT_GATE), dpl(0));
 */
/* HIGH-003 FIX: Added ist parameter so IST assignment is atomic with gate
 * write. Pass ist=1 for #DF (vector 8), ist=0 for all other vectors. */
void idt_set_gate(uint8_t vector, handler_addr_t handler, type_attr_t type_attr, dpl_t dpl,
                  uint8_t ist);

/**
 * @brief Send command to PIC
 *
 * @param port I/O port wrapper from io_port()
 * @param cmd Command byte
 *
 * @note Using io_port_t prevents accidentally swapping port with command
 * @note Internal function - not part of public API
 */
static inline void pic_send_command(io_port_t port, uint8_t cmd) {
    __asm__ volatile("outb %0, %1" ::"a"(cmd), "dN"(port.value));
}

/**
 * @brief Send data to PIC
 *
 * @param port I/O port wrapper from io_port()
 * @param data Data byte wrapper from io_data()
 *
 * @note Using io_port_t and io_data_t prevents parameter swapping
 * @note Internal function - not part of public API
 */
static inline void pic_send_data(io_port_t port, io_data_t data) {
    __asm__ volatile("outb %0, %1" ::"a"(data.value), "dN"(port.value));
}

/**
 * @brief Read data from PIC
 *
 * @param port I/O port wrapper from io_port()
 * @return Data byte read from port
 *
 * @note Internal function - not part of public API
 */
static inline uint8_t pic_read_data(io_port_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "dN"(port.value));
    return ret;
}

/* pic_send_eoi() removed — use irq_send_eoi() from irq.h (HIGH-001 fix). */

/**
 * @brief Disable PIC interrupts
 *
 * Masks all PIC interrupts.
 */
void pic_disable(void);

/**
 * @brief Enable interrupts (STI instruction)
 *
 * @note Use this instead of inline assembly for clarity
 */
void interrupts_enable(void);

/**
 * @brief Disable interrupts (CLI instruction)
 *
 * @note Use this instead of inline assembly for clarity
 */
void interrupts_disable(void);

/**
 * @brief Check if interrupts are enabled
 * @return 1 if enabled, 0 if disabled
 */
int interrupts_are_enabled(void);

/**
 * @brief Default exception handler (prints error and halts)
 *
 * @param frame Interrupt frame with register state
 */
void default_exception_handler(interrupt_frame_t* frame);

/**
 * @brief Default IRQ handler (sends EOI)
 *
 * @param frame Interrupt frame with register state
 */
void default_irq_handler(interrupt_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif /* IDT_H */
