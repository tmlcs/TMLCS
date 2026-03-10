#include "panic.h"
#include "print.h"
#include "serial.h"

/* ==========================================
 * panic() - Kernel panic with error code
 * ==========================================
 * 
 * @assembly
 *   Instrucción 1: cli
 *     - Propósito: Clear Interrupt Flag (deshabilitar interrupciones)
 *     - Efectos: IF bit en RFLAGS = 0
 *     - Ciclos: ~3
 *     - Barreras: Implícita (instrucción privilegiada)
 *     - Por qué: Prevenir que interrupciones interrumpan el panic
 *   
 *   Instrucción 2: hlt (en bucle infinito)
 *     - Propósito: Halt CPU until next external interrupt
 *     - Efectos: CPU entra en estado de bajo consumo
 *     - Ciclos: N/A (CPU detenido hasta interrupt)
 *     - Barreras: Implícita
 *     - Por qué: Detener CPU de forma segura, no ejecutar código basura
 * 
 * @note cli es necesario porque si STI estuviera activo, una interrupción
 *       podría intentar usar la consola mientras escribimos el panic
 * @note hlt en bucle infinito es el patrón estándar para kernel panic
 * @note Las interrupciones NMI (Non-Maskable) aún pueden ocurrir
 * 
 * @see sti() para habilitar interrupciones (NO llamar después de cli en panic)
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
    __asm__ volatile ("cli");

    /* ==========================================
     * Try to initialize serial if not ready
     * ========================================== */
    if (!serial_is_initialized()) {
        serial_init_default();
    }

    /* ==========================================
     * Error message via serial (always available)
     * ========================================== */
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

    /* ==========================================
     * Error message via VGA (if initialized)
     * ==========================================
     * HIGH-007: Use print_is_initialized() instead of print_detect()
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
     *   Instrucción: hlt
     *     - Propósito: Halt CPU until next external interrupt
     *     - Efectos: CPU entra en estado de bajo consumo, detiene ejecución
     *     - Ciclos: N/A (CPU detenido)
     *     - Barreras: Implícita
     *   
     *   Bucle for(;;):
     *     - Reinicia hlt después de cada interrupción
     *     - NMI (Non-Maskable Interrupts) pueden despertar el CPU
     *     - Reset hardware es la única forma de recuperar el sistema
     * 
     * @note hlt es preferible a un bucle empty porque:
     *   - Reduce consumo de energía
     *   - Previene ejecución de código basura
     *   - Permite debugging con hardware externo
     * @note Después de panic(), el sistema está muerto - solo reset lo recupera
     * 
     * @see https://www.felixcloutier.com/x86/hlt
     */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ==========================================
 * panic_simple() - Kernel panic without error code
 * ========================================== */
void panic_simple(const char* message) {
    panic(message, 0);
}
