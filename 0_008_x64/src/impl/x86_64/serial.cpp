#include "serial.h"
#include "print.h"
#include "hex_utils.h"
#include "decimal_utils.h"

/* ==========================================
 * Timeout Configuration
 * ==========================================
 * SERIAL_MAX_WAIT: Maximum iterations for busy-wait loops
 * This prevents infinite hangs if hardware fails to respond
 */
#define SERIAL_MAX_WAIT 100000

/* ==========================================
 * Serial Error Codes
 * ==========================================
 */
#define SERIAL_ERROR_NONE       0
#define SERIAL_ERROR_TIMEOUT    1
#define SERIAL_ERROR_INIT_FAIL  2
#define SERIAL_ERROR_NULL_PTR   3

/* ==========================================
 * Serial Driver State
 * ==========================================
 * VOLATILE: These variables track hardware state and must not
 * be optimized/cached by the compiler
 */
static volatile int serial_initialized = 0;
static volatile uint16_t serial_port = 0;
static volatile int serial_failed = 0;        /* CRITICAL: Separate flag for failures */
static volatile uint32_t serial_error_code = SERIAL_ERROR_NONE;
static volatile uint32_t serial_timeout_count = 0;

/* ==========================================
 * Low-Level I/O Functions
 * ========================================== */

/**
 * @brief Write a byte to a port
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Read a byte from a port
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
     * - Read IIR and verify it's not 0xFF (non-existent port)
     * - 0xFF typically indicates port not present (bus returns all-ones)
     */
    uint8_t iir = inb(port + SERIAL_IIR);

    /* 0xFF indicates non-existent port (bus floating) */
    if (iir == 0xFF) {
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
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_INIT_FAIL;
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
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_INIT_FAIL;
        return 0;  // Invalid baud rate: must be 110-115200
    }

    if (!is_valid_serial_port(port)) {
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_INIT_FAIL;
        return 0;  // Invalid port: must be COM1-COM4
    }

    /* Verify that the port physically exists */
    if (!serial_port_exists(port)) {
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_INIT_FAIL;
        return 0;  // Port does not exist or is not a UART
    }

    /* Reset error state on successful init */
    serial_failed = 0;
    serial_error_code = SERIAL_ERROR_NONE;
    serial_timeout_count = 0;

    /* Save port */
    serial_port = port;

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

    /* Small delay to ensure UART is ready */
    for (volatile int i = 0; i < 1000; i++) {
        __asm__ volatile ("nop");
    }

    serial_initialized = 1;

    return 1;
}

int serial_init_default(void) {
    return serial_init(SERIAL_DEFAULT_PORT, SERIAL_DEFAULT_BAUD);
}

int serial_is_initialized(void) {
    return serial_initialized;
}

/**
 * @brief Wait until ready to write with timeout
 * @param timeout Maximum number of iterations (0 = SERIAL_MAX_WAIT)
 * @return true if transmitter is empty, false if timeout
 *
 * Uses busy-wait with limit to prevent infinite hangs
 */
static bool serial_wait_transmit_empty_timeout(uint32_t timeout) {
    if (!serial_initialized) {
        return false;
    }

    if (timeout == 0) {
        timeout = SERIAL_MAX_WAIT;
    }

    /* Wait until THRE (bit 5) is set or timeout */
    while (timeout-- > 0) {
        if (inb(serial_port + SERIAL_LSR) & SERIAL_LSR_THRE) {
            return true;
        }
        /* Small delay to avoid bus saturation */
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
    serial_failed = 1;
    serial_error_code = SERIAL_ERROR_TIMEOUT;
    serial_timeout_count++;

    /* Report failure via VGA if available */
    if (print_detect()) {
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
    if (!serial_initialized) {
        return;  /* Serial not initialized or failed */
    }

    /* Wait until transmitter holding register is empty */
    if (!serial_wait_transmit_empty_timeout(SERIAL_MAX_WAIT)) {
        /* Timeout occurred - hardware may have failed */
        /* Do NOT retry indefinitely - let caller decide what to do */
        return;
    }

    /* Write the character */
    outb(serial_port + SERIAL_THR, (uint8_t)data);
}

void serial_write_str(const char* str) {
    /* 
     * CRIT-003 FIX: Null pointer is a programming error, NOT hardware failure.
     * Do NOT set serial_failed or increment timeout counters.
     * Just record the error code for diagnostics and return.
     */
    if (str == nullptr) {
        serial_error_code = SERIAL_ERROR_NULL_PTR;
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
        serial_write_dec64((uint64_t)(-(int64_t)value));
    } else {
        serial_write_dec((uint32_t)value);
    }
}

/**
 * @brief Write a 64-bit signed integer in decimal to serial
 * @param value 64-bit signed value to write
 */
void serial_write_dec64_signed(int64_t value) {
    if (value < 0) {
        serial_write_char('-');
        serial_write_dec64((uint64_t)(-value));
    } else {
        serial_write_dec64((uint64_t)value);
    }
}

int serial_read_char(char* data) {
    if (!serial_initialized) {
        return 0;
    }
    
    if (data == nullptr) {
        return 0;
    }

    /* Check if data is available (DR bit) */
    if (inb(serial_port + SERIAL_LSR) & SERIAL_LSR_DR) {
        *data = (char)inb(serial_port + SERIAL_RBR);
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
 */
int serial_has_failed(void) {
    return serial_failed;
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
 */
uint32_t serial_get_error_code(void) {
    return serial_error_code;
}

/**
 * @brief Get the count of timeout errors
 * @return Number of timeouts since initialization
 *
 * This counter increments each time a serial operation times out.
 * Useful for diagnosing intermittent hardware issues.
 */
uint32_t serial_get_timeout_count(void) {
    return serial_timeout_count;
}

/**
 * @brief Clear the serial error state
 *
 * Resets the failed flag and error code.
 * Useful for recovery attempts or re-initialization.
 */
void serial_clear_error(void) {
    serial_failed = 0;
    serial_error_code = SERIAL_ERROR_NONE;
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
