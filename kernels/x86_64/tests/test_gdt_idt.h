#ifndef TEST_GDT_IDT_H
#define TEST_GDT_IDT_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GDT/IDT Test Module Header
 * =============================================================================
 * Tests for Global Descriptor Table and Interrupt Descriptor Table.
 * =============================================================================
 */

/**
 * @brief Test GDT initialization
 *
 * Verifies:
 *   - GDT is properly initialized
 *   - GDT entries are correctly configured
 *   - Kernel code/data selectors are valid
 */
void test_gdt_initialization(void);

/**
 * @brief Test IDT initialization
 *
 * Verifies:
 *   - IDT is properly initialized
 *   - Exception handlers are registered (INT 0-19)
 *   - IRQ handlers are registered (INT 0x20-0x2F)
 *   - PIC is remapped correctly
 */
void test_idt_initialization(void);

/**
 * @brief Test interrupt enable/disable
 *
 * Verifies:
 *   - interrupts_enable() works
 *   - interrupts_disable() works
 *   - interrupts_are_enabled() returns correct state
 */
void test_interrupt_control(void);

/**
 * @brief Test breakpoint exception (INT 3)
 *
 * Triggers a software breakpoint and verifies the exception handler works.
 *
 * @note This is a safe exception that won't crash the system
 */
void test_breakpoint_exception(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_GDT_IDT_H */
