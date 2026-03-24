#include "panic.h"
#include "print.h"
#include "serial.h"

/* ==========================================
 * panic() - Kernel panic with error code
 * ==========================================
 *
 * @assembly
 *   Instruction 1: cli
 *     - Purpose: Clear Interrupt Flag (disable interrupts)
 *     - Effects: IF bit in RFLAGS = 0
 *     - Cycles: ~3
 *     - Barriers: Implicit (privileged instruction)
 *     - Why: Prevent interrupts from interrupting the panic
 *
 *   Instruction 2: hlt (in infinite loop)
 *     - Purpose: Halt CPU until next external interrupt
 *     - Effects: CPU enters low-power state
 *     - Cycles: N/A (CPU halted until interrupt)
 *     - Barriers: Implicit
 *     - Why: Stop CPU safely, do not execute garbage code
 *
 * @note cli is necessary because if STI were active, an interrupt
 *       could try to use the console while we write the panic
 * @note hlt in infinite loop is the standard pattern for kernel panic
 * @note NMI (Non-Maskable) interrupts can still occur
 *
 * @see sti() to enable interrupts (DO NOT call after cli in panic)
 * @see https://www.felixcloutier.com/x86/cli
 * @see https://www.felixcloutier.com/x86/hlt
 * ==========================================
 */
void panic(const char* message, uint32_t error_code) {
    /* ==========================================
     * Disable interrupts
     * ==========================================
     * CRITICAL: Must disable interrupts before any output
     * to prevent reentrancy issues if an interrupt handler
     * tries to use serial/print functions
     */
    __asm__ volatile("cli");

    /* HIGH-NEW-001 FIX: Force-release serial lock before any output.
     *
     * If panic() is called while g_serial_lock is held (e.g., a fault
     * fires inside serial_write_str()), spinlock_acquire() in the serial
     * output path would spin forever — deadlock.
     *
     * Preconditions are met: interrupts are disabled (cli above) and
     * this is a fatal non-returning path, so breaking mutual exclusion
     * is safe here.
     */
    serial_force_unlock();

    /* ==========================================
     * Error message via serial (if initialized)
     * ==========================================
     * Note: Serial is initialized early in kernel_main() before
     * any code that can panic. If serial failed at boot, it will
     * not work here either. Do NOT attempt re-initialization:
     *   - serial_init_default() with interrupts disabled could hang
     *   - If serial hardware is broken, we'd waste time retrying
     *   - The panic message should be as fast as possible
     *
     * If serial was not initialized, skip serial output and rely
     * on VGA (if available) or just halt.
     */
    if (serial_is_initialized()) {
        serial_write_str("\r\n\r\n");
        serial_write_str("!!! KERNEL PANIC !!!\r\n");
        serial_write_str("\r\n");

        if (message != nullptr) {
            serial_write_str("Error: ");
            serial_write_str(message);
            serial_write_str("\r\n");
        }

        serial_write_str("Error Code: 0x");
        serial_write_hex(error_code);
        serial_write_str("\r\n");
        serial_write_str("\r\nSystem halted.\r\n");
    }

    /* ==========================================
     * Error message via VGA (if initialized)
     * ==========================================
     * Use print_is_initialized() instead of print_detect()
     * to ensure VGA is fully initialized before using print_clear().
     * print_detect() only tests hardware presence, but print_clear()
     * requires full initialization.
     * ========================================== */
    if (print_is_initialized()) {
        /* Red background, bright white text for maximum contrast */
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
        print_clear();

        print_str("!!! KERNEL PANIC !!!\r\n");
        print_str("\r\n");

        if (message != nullptr) {
            print_str("Error: ");
            print_str(message);
            print_str("\r\n");
        }

        print_str("Error Code: 0x");
        print_hex(error_code);
        print_str("\r\n");
        print_str("\r\nSystem halted.\r\n");
    }

    /* ==========================================
     * Infinite loop with HLT - never return
     * ==========================================
     *
     * @assembly
     *   Instruction: hlt
     *     - Purpose: Halt CPU until next external interrupt
     *     - Effects: CPU enters low-power state, stops execution
     *     - Cycles: N/A (CPU halted)
     *     - Barriers: Implicit
     *
     *   for(;;) loop:
     *     - Restarts hlt after each interrupt
     *     - NMI (Non-Maskable Interrupts) can wake the CPU
     *     - Hardware reset is the only way to recover the system
     *
     * @note hlt is preferable to an empty loop because:
     *   - Reduces power consumption
     *   - Prevents execution of garbage code
     *   - Allows debugging with external hardware
     * @note After panic(), the system is dead - only reset recovers it
     *
     * @see https://www.felixcloutier.com/x86/hlt
     */
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* ==========================================
 * panic_simple() - Kernel panic without error code
 * ========================================== */
void panic_simple(const char* message) {
    panic(message, 0);
}
