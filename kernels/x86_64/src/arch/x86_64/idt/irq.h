#ifndef IRQ_H
#define IRQ_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * IRQ (Interrupt Request) Handler System
 * =============================================================================
 * 
 * This module manages hardware interrupt handlers for the 8259 PIC.
 * 
 * IRQ Mapping (Master + Slave PIC):
 *   IRQ0  - Timer (PIT)
 *   IRQ1  - Keyboard
 *   IRQ2  - Cascade (slave PIC)
 *   IRQ3  - COM2 (Serial)
 *   IRQ4  - COM1 (Serial)
 *   IRQ5  - LPT2 (Parallel)
 *   IRQ6  - Floppy Disk
 *   IRQ7  - LPT1 (Parallel) / Spurious
 *   IRQ8  - RTC (Real-time clock)
 *   IRQ9  - ACPI / Free
 *   IRQ10 - Free / Network
 *   IRQ11 - Free / Network
 *   IRQ12 - PS/2 Mouse
 *   IRQ13 - FPU / Math coprocessor
 *   IRQ14 - Primary ATA
 *   IRQ15 - Secondary ATA
 * 
 * IDT Entries:
 *   IRQ0-7  -> IDT 32-39 (0x20-0x27)
 *   IRQ8-15 -> IDT 40-47 (0x28-0x2F)
 * =============================================================================
 */

/* =============================================================================
 * IRQ Constants
 * =============================================================================
 */
#define IRQ_BASE_MASTER   0x20    /* Master PIC IRQ base in IDT */
#define IRQ_BASE_SLAVE    0x28    /* Slave PIC IRQ base in IDT */
#define IRQ_COUNT         16      /* Total IRQ lines */

/* PIC I/O Ports */
#define PIC1_COMMAND      0x20    /* Master PIC command port */
#define PIC1_DATA         0x21    /* Master PIC data port */
#define PIC2_COMMAND      0xA0    /* Slave PIC command port */
#define PIC2_DATA         0xA1    /* Slave PIC data port */

/* PIC Commands */
#define PIC_EOI           0x20    /* End of Interrupt command */

/* PIC Masks */
#define PIC_MASK_ALL      0xFF    /* Mask all IRQs */
#define PIC_MASK_NONE     0x00    /* Unmask all IRQs */

/* =============================================================================
 * IRQ Handler Type
 * =============================================================================
 */
typedef void (*irq_handler_t)(void);

/* =============================================================================
 * IRQ API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize PIC and IRQ system
 * @return 1 on success, 0 on failure
 * 
 * Remaps PIC to use IDT entries 32-47 (0x20-0x2F)
 * This avoids conflict with CPU exceptions (0-31).
 */
int irq_init(void);

/**
 * @brief Check if IRQ system is initialized
 * @return 1 if initialized, 0 otherwise
 */
int irq_is_initialized(void);

/**
 * @brief Shutdown IRQ system (disable all IRQs)
 */
void irq_shutdown(void);

/* =============================================================================
 * IRQ API - Handler Registration
 * =============================================================================
 */

/**
 * @brief Register an IRQ handler
 * @param irq IRQ number (0-15)
 * @param handler Handler function
 * @return 1 on success, 0 on failure
 * 
 * Handler is called when the IRQ fires.
 * Only one handler per IRQ is supported.
 */
int irq_register_handler(uint8_t irq, irq_handler_t handler);

/**
 * @brief Unregister an IRQ handler
 * @param irq IRQ number (0-15)
 * @return 1 on success, 0 on failure
 */
int irq_unregister_handler(uint8_t irq);

/**
 * @brief Get current handler for an IRQ
 * @param irq IRQ number (0-15)
 * @return Handler function or nullptr if none
 */
irq_handler_t irq_get_handler(uint8_t irq);

/* =============================================================================
 * IRQ API - Mask Control
 * =============================================================================
 */

/**
 * @brief Enable (unmask) a specific IRQ
 * @param irq IRQ number (0-15)
 */
void irq_enable(uint8_t irq);

/**
 * @brief Disable (mask) a specific IRQ
 * @param irq IRQ number (0-15)
 */
void irq_disable(uint8_t irq);

/**
 * @brief Check if an IRQ is enabled
 * @param irq IRQ number (0-15)
 * @return 1 if enabled, 0 if disabled
 */
int irq_is_enabled(uint8_t irq);

/**
 * @brief Enable all IRQs
 */
void irq_enable_all(void);

/**
 * @brief Disable all IRQs
 */
void irq_disable_all(void);

/* =============================================================================
 * IRQ API - Interrupt Handling
 * =============================================================================
 */

/**
 * @brief Main IRQ dispatcher
 * @param irq IRQ number that fired
 * 
 * Called from assembly IRQ stub.
 * Dispatches to registered handler and sends EOI.
 */
void irq_dispatch(uint8_t irq);

/**
 * @brief Send End of Interrupt to PIC
 * @param irq IRQ number that was handled
 * 
 * Must be called after handling an IRQ.
 * Tells PIC that interrupt processing is complete.
 */
void irq_send_eoi(uint8_t irq);

/* =============================================================================
 * IRQ API - Debug/Testing
 * =============================================================================
 */

/**
 * @brief Print IRQ status
 * 
 * Shows enabled/disabled IRQs and registered handlers.
 */
void irq_print_status(void);

/**
 * @brief Get IRQ statistics
 * @param irq IRQ number (0-15)
 * @return Number of times IRQ has fired
 */
uint32_t irq_get_count(uint8_t irq);

/**
 * @brief Reset IRQ statistics
 */
void irq_reset_counts(void);

/* =============================================================================
 * Assembly IRQ Stubs (extern)
 * =============================================================================
 * These are defined in interrupts.asm and jump to irq_dispatch()
 */

/* Master PIC IRQs (IRQ0-7) */
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq2_stub(void);
extern void irq3_stub(void);
extern void irq4_stub(void);
extern void irq5_stub(void);
extern void irq6_stub(void);
extern void irq7_stub(void);

/* Slave PIC IRQs (IRQ8-15) */
extern void irq8_stub(void);
extern void irq9_stub(void);
extern void irq10_stub(void);
extern void irq11_stub(void);
extern void irq12_stub(void);
extern void irq13_stub(void);
extern void irq14_stub(void);
extern void irq15_stub(void);

#ifdef __cplusplus
}
#endif

#endif /* IRQ_H */
