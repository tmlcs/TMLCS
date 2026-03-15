#include "serial.h"
#include "print.h"
#include "hex_utils.h"
#include "decimal_utils.h"
#include "constants.h"

/* ==========================================
 * Memory Barriers for SMP Safety
 * ==========================================
 * volatile prevents compiler optimization but doesn't guarantee:
 *   - Atomicity of read-modify-write operations
 *   - Memory ordering across CPUs
 *   - Visibility of writes to other CPUs
 *
 * These barriers ensure proper ordering on x86_64:
 *   - mb()  : Full memory barrier (load + store)
 *   - rmb() : Read memory barrier (load ordering)
 *   - wmb() : Write memory barrier (store ordering)
 *
 * Note: x86_64 has strong memory ordering, but barriers are still
 * needed for SMP correctness and to prevent compiler reordering.
 *
 * @assembly
 *   Instrucción: "" (empty inline assembly)
 *   Clobbers: "memory" - tells compiler that memory may be modified
 *   Propósito: Prevenir reordenamiento de instrucciones por el compilador
 *   Efectos: 
 *     - mb()  : Barrera completa (lectura + escritura)
 *     - rmb() : Barrera de lectura (ordenamiento de loads)
 *     - wmb() : Barrera de escritura (ordenamiento de stores)
 *   Ciclos: 0 (es una directiva al compilador, no genera código)
 *   Barreras: Explícita vía clobber "memory"
 * 
 * @note En x86_64, el hardware tiene ordenamiento fuerte, pero el
 *       compilador puede reordenar instrucciones. Esta barrera lo previene.
 * @note El clobber "memory" le dice al compilador que cualquier acceso
 *       a memoria debe completarse antes de continuar.
 * 
 * @see mb(), rmb(), wmb() macros
 */
#define mb()  __asm__ volatile ("" ::: "memory")
#define rmb() __asm__ volatile ("" ::: "memory")
#define wmb() __asm__ volatile ("" ::: "memory")

/* ==========================================
 * Timeout Configuration
 * ==========================================
 * SERIAL_MAX_WAIT: Maximum iterations for busy-wait loops
 * This prevents infinite hangs if hardware fails to respond
 *
 * Note: Using SERIAL_MAX_TIMEOUT from constants.h for consistency
 */
#define SERIAL_MAX_WAIT  SERIAL_MAX_TIMEOUT

/* ==========================================
 * Serial Driver State
 * ==========================================
 * VOLATILE: This structure tracks hardware state and must not
 * be optimized/cached by the compiler
 *
 * For SMP safety, all accesses to this structure
 * must use memory barriers (mb/rmb/wmb) to ensure:
 *   - Proper ordering of reads/writes across CPUs
 *   - Visibility of changes to all processors
 *   - Atomicity for compound operations (protected by barriers)
 * 
 * Encapsulation Benefits:
 *   - All state in one place (easier to debug)
 *   - Can be passed to functions (easier to test)
 *   - Clear interface via getter functions
 */
static SerialState_t g_serial_state;  /* Zero-initialized by default (BSS) */

/* ==========================================
 * Internal State Access Functions
 * ==========================================
 * These functions provide controlled access to the serial state.
 * In SMP, these would acquire the serial spinlock.
 */

/**
 * @brief Get pointer to serial state (internal use)
 * @return Pointer to serial state structure
 * @note For internal driver use only
 */
static inline SerialState_t* get_serial_state(void) {
    return &g_serial_state;
}

/* ==========================================
 * Low-Level I/O Functions
 * ==========================================
 */

/**
 * @brief Write a byte to a port
 * 
 * @assembly
 *   Instrucción: outb
 *   Operandos: 
 *     - %0 (output): AL register (valor a enviar)
 *     - %1 (input): DX register (puerto de E/S)
 *   Constraint "a": Usa el registro AL/AX/EAX/RAX
 *   Constraint "Nd": Puerto inmediato (0-255) o registro DX
 *   Efectos: Escribe byte en puerto de E/S especificado
 *   Ciclos: ~100-1000 (depende del dispositivo de E/S)
 *   Barreras: Implícita (volatile previene reordenamiento)
 * 
 * @note Esta función es específica de x86/x86_64
 * @note No puede ser inlinada completamente debido a volatile
 * @note Los puertos de E/S son espacios separados de memoria (I/O mapped)
 * @note El compilador no puede reordenar esta instrucción debido a volatile
 * 
 * @param port Puerto de E/S (ej: 0x3F8 para COM1)
 * @param value Byte a enviar
 * 
 * @see inb() para lectura de puertos
 * @see SERIAL_COM1, SERIAL_COM2 para puertos estándar
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Read a byte from a port
 * 
 * @assembly
 *   Instrucción: inb
 *   Operandos:
 *     - %0 (output): AL register (valor leído)
 *     - %1 (input): DX register (puerto de E/S)
 *   Constraint "=a": Escribe en AL/AX/EAX/RAX
 *   Constraint "Nd": Puerto inmediato (0-255) o registro DX
 *   Efectos: Lee byte desde puerto de E/S especificado
 *   Ciclos: ~100-1000 (depende del dispositivo de E/S)
 *   Barreras: Implícita (volatile previene reordenamiento)
 * 
 * @note Esta función es específica de x86/x86_64
 * @note El valor de retorno está en el registro AL después de la instrucción
 * @note Los puertos de E/S son espacios separados de memoria (I/O mapped)
 * @note El compilador no puede reordenar esta instrucción debido a volatile
 * 
 * @param port Puerto de E/S (ej: 0x3F8 para COM1)
 * @return uint8_t Byte leído desde el puerto
 * 
 * @see outb() para escritura de puertos
 * @see SERIAL_COM1, SERIAL_COM2 para puertos estándar
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ==========================================
 * Port Validation
 * ========================================== */

/**
 * @brief Check if serial port is valid
 * @param port Port to check
 * @return true if COM1-COM4, false otherwise
 */
static bool is_valid_serial_port(uint16_t port) {
    return (port == SERIAL_COM1 ||
            port == SERIAL_COM2 ||
            port == SERIAL_COM3 ||
            port == SERIAL_COM4);
}

/**
 * @brief Check if serial port exists (UART 16550+)
 * @param port Port to check
 * @return true if port responds as UART, false otherwise
 *
 * Checks bits 6-7 of IIR register which should be 1 on UART 16550+
 * Note: In QEMU and some embedded systems, this check may not be
 * reliable. Used as optimization but not critical.
 */
static bool serial_port_exists(uint16_t port) {
    /*
     * UART 16550+ has bits 6-7 of IIR at 0xC0 when no interrupts
     * However, some systems (QEMU, older hardware) may return
     * other values. We use a more permissive check:
     * - Read IIR and verify it's not SERIAL_IIR_NO_DEVICE (non-existent port)
     * - SERIAL_IIR_NO_DEVICE typically indicates port not present (bus floating)
     */
    uint8_t iir = inb(port + SERIAL_IIR);

    /* SERIAL_IIR_NO_DEVICE indicates non-existent port (bus floating) */
    if (iir == SERIAL_IIR_NO_DEVICE) {
        return false;
    }

    /* Any other value indicates port present */
    return true;
}

/* ==========================================
 * Public Function Implementation
 * ========================================== */

int serial_init(uint16_t port, uint32_t baud) {
    /* ==========================================
     * Parameter Validation - CRITICAL
     * Prevent division by zero and invalid I/O
     * ========================================== */

    /* Validate baud rate is not zero */
    if (baud == 0) {
        wmb();  /* Ensure all prior writes complete */
        g_serial_state.failed = 1;
        g_serial_state.error_code = SERIAL_ERROR_INIT_FAIL;
        wmb();  /* Ensure state writes are visible */
        return 0;  // Invalid parameter: baud rate cannot be zero
    }

    /* ==========================================
     * Validate baud rate range [HIGH-004]
     * ==========================================
     * Standard UART baud rates:
     *   - Minimum: 110 baud (divisor = 1047)
     *   - Maximum: 115200 baud (divisor = 1)
     *
     * Common rates: 110, 300, 600, 1200, 2400, 4800, 9600,
     *               14400, 19200, 38400, 57600, 115200
     *
     * Out-of-range rates cause:
     *   - Very low baud: divisor > 65535 (truncation, wrong rate)
     *   - Very high baud: divisor = 0 (undefined behavior)
     * ========================================== */
    if (baud < 110 || baud > 115200) {
        wmb();
        g_serial_state.failed = 1;
        g_serial_state.error_code = SERIAL_ERROR_INIT_FAIL;
        wmb();
        return 0;  // Invalid baud rate: must be 110-115200
    }

    if (!is_valid_serial_port(port)) {
        wmb();
        g_serial_state.failed = 1;
        g_serial_state.error_code = SERIAL_ERROR_INIT_FAIL;
        wmb();
        return 0;  // Invalid port: must be COM1-COM4
    }

    /* Verify that the port physically exists */
    if (!serial_port_exists(port)) {
        wmb();
        g_serial_state.failed = 1;
        g_serial_state.error_code = SERIAL_ERROR_INIT_FAIL;
        wmb();
        return 0;  // Port does not exist or is not a UART
    }

    /* Reset error state on successful init */
    wmb();  /* Clear prior state */
    g_serial_state.failed = 0;
    g_serial_state.error_code = SERIAL_ERROR_NONE;
    g_serial_state.timeout_count = 0;
    wmb();  /* Ensure state is visible before proceeding */

    /* Save port */
    g_serial_state.port = port;

    /* Disable interrupts */
    outb(port + SERIAL_IER, 0x00);

    /* Enable DLAB to configure baud rate divisor */
    outb(port + SERIAL_LCR, SERIAL_LCR_DLAB);

    /* Calculate divisor for baud rate
     * Formula: divisor = 115200 / baud
     * For 115200: divisor = 1
     * For 9600: divisor = 12
     * For 110: divisor = 1047
     * Note: baud is already validated in range 110-115200
     */
    uint16_t divisor = 115200 / baud;
    outb(port + SERIAL_DLL, (divisor & 0xFF));       /* Low byte */
    outb(port + SERIAL_DLM, (divisor >> 8) & 0xFF);  /* High byte */

    /* Configure 8 bits, no parity, 1 stop bit (8N1) and disable DLAB */
    outb(port + SERIAL_LCR, SERIAL_LCR_8N1);

    /* Enable FIFOs (16550), clear them, set 14 byte threshold */
    outb(port + SERIAL_FCR, 0x07);

    /* Configure modem: DTR + RTS + OUT2 (enable interrupts) */
    outb(port + SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);

    /* Clear receive buffer by reading any pending data */
    (void)inb(port + SERIAL_RBR);

    /* Small delay to ensure UART is ready
     * 
     * @assembly
     *   Instrucción: nop (No Operation)
     *   Operandos: Ninguno
     *   Efectos: Ninguno - solo consume 1 ciclo de CPU
     *   Ciclos: 1
     *   Propósito: Pequeño delay para estabilizar hardware
     * 
     * @note Se usa volatile en el contador para prevenir optimización
     * @note 1000 nops = ~1000 ciclos = ~0.5ms en 2GHz
     */
    for (volatile int i = 0; i < SERIAL_INIT_DELAY_ITERATIONS; i++) {
        __asm__ volatile ("nop");
    }

    wmb();  /* Ensure all prior writes complete */
    g_serial_state.initialized = 1;
    mb();   /* Full barrier - initialization complete and visible */

    return 1;
}

int serial_init_default(void) {
    return serial_init(SERIAL_DEFAULT_PORT, SERIAL_DEFAULT_BAUD);
}

/**
 * @brief Check if serial port is initialized
 * @return 1 if initialized, 0 otherwise
 * 
 * @note This function accesses the encapsulated state structure
 * @note For SMP safety, uses memory barrier to ensure latest value
 */
int serial_is_initialized(void) {
    rmb();  /* Ensure we see latest state */
    return g_serial_state.initialized;
}

/**
 * @brief Wait until ready to write with timeout
 * @param timeout Maximum number of iterations (0 = SERIAL_MAX_WAIT)
 * @return true if transmitter is empty, false if timeout
 *
 * Uses busy-wait with limit to prevent infinite hangs
 */
static bool serial_wait_transmit_empty_timeout(uint32_t timeout) {
    rmb();  /* Ensure we see latest state */
    if (!g_serial_state.initialized) {
        return false;
    }

    if (timeout == 0) {
        timeout = SERIAL_MAX_WAIT;
    }

    /* Wait until THRE (bit 5) is set or timeout */
    while (timeout-- > 0) {
        if (inb(g_serial_state.port + SERIAL_LSR) & SERIAL_LSR_THRE) {
            return true;
        }
        /* Small delay to avoid bus saturation
         * 
         * @assembly
         *   Instrucción: nop (No Operation)
         *   Operandos: Ninguno
         *   Efectos: Ninguno - solo consume 1 ciclo de CPU
         *   Ciclos: 1
         *   Propósito: Prevenir saturación del bus de E/S con lecturas continuas
         * 
         * @note Sin este nop, el bucle leería el puerto miles de veces por milisegundo
         * @note En hardware real, esto puede causar problemas de timing
         */
        __asm__ volatile ("nop");
    }

    /* ==========================================
     * TIMEOUT - Hardware failure detected
     * ==========================================
     * CRITICAL: Do NOT silently clear serial_initialized.
     * Instead:
     *   1. Set serial_failed flag
     *   2. Record error code
     *   3. Increment timeout counter for diagnostics
     *   4. Report failure via VGA if available
     * ========================================== */
    wmb();  /* Ensure ordering of failure state */
    g_serial_state.failed = 1;
    g_serial_state.error_code = SERIAL_ERROR_TIMEOUT;
    g_serial_state.timeout_count++;  /* Note: RMW not atomic without lock */
    wmb();  /* Ensure failure state is visible */

    /* Use print_is_initialized() instead of print_detect()
     * print_detect() performs a hardware test write which may not be safe
     * if called before VGA is fully initialized.
     * print_is_initialized() simply checks the initialization flag.
     */
    rmb();  /* Ensure we see latest print state */
    if (print_is_initialized()) {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        print_str("[SERIAL TIMEOUT] Hardware not responding!\r\n");
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
    }

    return false;
}

void serial_wait_transmit_empty(void) {
    /* Wrapper that uses default timeout */
    serial_wait_transmit_empty_timeout(SERIAL_MAX_WAIT);
}

void serial_write_char(char data) {
    /* Check if serial is initialized OR failed (for graceful degradation) */
    rmb();  /* Ensure we see latest state */
    if (!g_serial_state.initialized) {
        return;  /* Serial not initialized or failed */
    }

    /* Wait until transmitter holding register is empty */
    if (!serial_wait_transmit_empty_timeout(SERIAL_MAX_WAIT)) {
        /* Timeout occurred - hardware may have failed */
        /* Do NOT retry indefinitely - let caller decide what to do */
        return;
    }

    /* Write the character */
    outb(g_serial_state.port + SERIAL_THR, (uint8_t)data);
}

void serial_write_str(const char* str) {
    /*
     * Null pointer is a programming error, NOT hardware failure.
     * Do NOT set serial_failed or increment timeout counters.
     * Just record the error code for diagnostics and return.
     */
    if (str == nullptr) {
        wmb();
        g_serial_state.error_code = SERIAL_ERROR_NULL_PTR;
        wmb();
        /* Do NOT set serial_failed = 1 - this is not a hardware error */
        return;
    }

    while (*str) {
        serial_write_char(*str);
        str++;
    }
}

void serial_write_hex(uint32_t value) {
    char buffer[11];  /* "0x" + 8 digits + null */
    
    // Use shared utility function from hex_utils.h (DRY principle)
    uint32_to_hex_string(buffer, value);
    
    serial_write_str(buffer);
}

void serial_write_dec(uint32_t value) {
    char buffer[12];  /* Maximum 10 digits + null */

    // Use shared utility function (DRY principle)
    serial_write_str(uint32_to_decimal_string(buffer, value));
}

/**
 * @brief Write a 64-bit unsigned integer in hexadecimal to serial
 * @param value 64-bit value to write
 */
void serial_write_hex64(uint64_t value) {
    char buffer[19];  // "0x" + 16 digits + null = 19 bytes

    // Use shared utility function from hex_utils.h (DRY principle)
    uint64_to_hex_string(buffer, value);

    serial_write_str(buffer);
}

/**
 * @brief Write a 64-bit unsigned integer in decimal to serial
 * @param value 64-bit value to write
 */
void serial_write_dec64(uint64_t value) {
    char buffer[22];  // Maximum 20 digits + null

    // Use shared utility function (DRY principle)
    serial_write_str(uint64_to_decimal_string(buffer, value));
}

/**
 * @brief Write a 32-bit signed integer in decimal to serial
 * @param value Signed value to write
 */
void serial_write_dec_signed(int32_t value) {
    if (value < 0) {
        serial_write_char('-');
        /* Use two's complement to avoid undefined behavior.
         * For INT32_MIN (-2147483648), negation would overflow in signed arithmetic.
         * Casting to uint64_t first, then negating in unsigned arithmetic is safe.
         */
        serial_write_dec64(0 - static_cast<uint64_t>(value));
    } else {
        serial_write_dec(static_cast<uint32_t>(value));
    }
}

/**
 * @brief Write a 64-bit signed integer in decimal to serial
 * @param value 64-bit signed value to write
 */
void serial_write_dec64_signed(int64_t value) {
    if (value < 0) {
        serial_write_char('-');
        /* Use two's complement to avoid undefined behavior.
         * For INT64_MIN (-9223372036854775808), negation would overflow in signed arithmetic.
         * Casting to uint64_t first, then negating in unsigned arithmetic is safe.
         */
        serial_write_dec64(0 - static_cast<uint64_t>(value));
    } else {
        serial_write_dec64(static_cast<uint64_t>(value));
    }
}

int serial_read_char(char* data) {
    rmb();  /* Ensure we see latest state */
    if (!g_serial_state.initialized) {
        return 0;
    }

    if (data == nullptr) {
        return 0;
    }

    /* Check if data is available (DR bit) */
    if (inb(g_serial_state.port + SERIAL_LSR) & SERIAL_LSR_DR) {
        *data = (char)inb(g_serial_state.port + SERIAL_RBR);
        return 1;
    }

    return 0;
}

/* ==========================================
 * Public API - Error Reporting Functions
 * ==========================================
 * These functions allow the kernel to diagnose serial port issues
 * without silent failures.
 * ========================================== */

/**
 * @brief Check if serial port has failed
 * @return 1 if failed, 0 if OK or not initialized
 *
 * A failed serial port may still have been initialized successfully,
 * but encountered a hardware error during operation (e.g., timeout).
 * 
 * @note This function accesses the encapsulated state structure
 * @note For SMP safety, uses memory barrier to ensure latest value
 */
int serial_has_failed(void) {
    rmb();  /* Ensure we see latest state */
    return g_serial_state.failed;
}

/**
 * @brief Get the last serial error code
 * @return Error code (0 = none, 1 = timeout, 2 = init fail, 3 = null ptr)
 *
 * Error codes:
 *   SERIAL_ERROR_NONE (0)      - No error
 *   SERIAL_ERROR_TIMEOUT (1)   - Hardware timeout (not responding)
 *   SERIAL_ERROR_INIT_FAIL (2) - Initialization failed
 *   SERIAL_ERROR_NULL_PTR (3)  - Null pointer passed to function
 *   
 * @note This function accesses the encapsulated state structure
 * @note For SMP safety, uses memory barrier to ensure latest value
 */
uint32_t serial_get_error_code(void) {
    rmb();  /* Ensure we see latest state */
    return g_serial_state.error_code;
}

/**
 * @brief Get the count of timeout errors
 * @return Number of timeouts since initialization
 *
 * This counter increments each time a serial operation times out.
 * Useful for diagnosing intermittent hardware issues.
 * 
 * @note This function accesses the encapsulated state structure
 * @note For SMP safety, uses memory barrier to ensure latest value
 */
uint32_t serial_get_timeout_count(void) {
    rmb();  /* Ensure we see latest state */
    return g_serial_state.timeout_count;
}

/**
 * @brief Clear the serial error state
 *
 * Resets the failed flag and error code.
 * Useful for recovery attempts or re-initialization.
 */
void serial_clear_error(void) {
    wmb();  /* Ensure ordering */
    g_serial_state.failed = 0;
    g_serial_state.error_code = SERIAL_ERROR_NONE;
    wmb();  /* Ensure cleared state is visible */
}

/* ==========================================
 * Re-initialization after timeout
 * ==========================================
 * These functions allow recovery from timeout errors by properly
 * clearing the failed state before re-initializing the hardware.
 * ==========================================
 */

/**
 * @brief Re-initialize serial port after a timeout or failure
 * @param port Serial port (e.g., SERIAL_COM1)
 * @param baud Baud rate (e.g., 115200)
 * @return 1 if success, 0 if failure
 *
 * This function allows recovery from timeout errors by:
 *   1. Clearing the failed state
 *   2. Resetting error counters
 *   3. Re-initializing the hardware
 *
 * Use this when:
 *   - serial_has_failed() returns 1
 *   - serial_get_error_code() returns SERIAL_ERROR_TIMEOUT
 *   - You want to retry initialization without rebooting
 *
 * @note This is different from serial_init() - it properly clears
 *       the failed state before re-initializing.
 */
int serial_reinit(uint16_t port, uint32_t baud) {
    /* ==========================================
     * Clear failed state BEFORE re-init
     * ==========================================
     * serial_init() checks serial_failed and may return early
     * if the port is marked as failed. We must clear the state
     * to allow a fresh initialization attempt.
     * ========================================== */
    wmb();  /* Ensure ordering */
    g_serial_state.failed = 0;
    g_serial_state.error_code = SERIAL_ERROR_NONE;
    g_serial_state.timeout_count = 0;  /* Reset timeout counter for fresh start */
    g_serial_state.initialized = 0;    /* Clear initialized flag for fresh init */
    wmb();  /* Ensure cleared state is visible before re-init */

    /* Now perform normal initialization */
    return serial_init(port, baud);
}

/**
 * @brief Re-initialize COM1 with default baud rate
 * @return 1 if success, 0 if failure
 *
 * Convenience wrapper for serial_reinit(SERIAL_DEFAULT_PORT, SERIAL_DEFAULT_BAUD).
 */
int serial_reinit_default(void) {
    return serial_reinit(SERIAL_DEFAULT_PORT, SERIAL_DEFAULT_BAUD);
}

/**
 * @brief Get a human-readable error message
 * @return Static string describing the error
 *
 * @note Returns English strings only.
 * @note String is static - do not free or modify.
 */
const char* serial_get_error_string(uint32_t error_code) {
    switch (error_code) {
        case SERIAL_ERROR_NONE:
            return "No error";
        case SERIAL_ERROR_TIMEOUT:
            return "Serial hardware timeout (not responding)";
        case SERIAL_ERROR_INIT_FAIL:
            return "Serial initialization failed";
        case SERIAL_ERROR_NULL_PTR:
            return "Null pointer argument";
        default:
            return "Unknown error code";
    }
}
