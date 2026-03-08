#include "serial.h"
#include "print.h"
#include "hex_utils.h"

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
 * Estado del driver serial
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
 * Funciones de I/O de bajo nivel
 * ========================================== */

/**
 * @brief Escribir un byte a un puerto
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Leer un byte de un puerto
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ==========================================
 * Validación de puertos
 * ========================================== */

/**
 * @brief Verificar si el puerto serial es válido
 * @param port Puerto a verificar
 * @return true si es COM1-COM4, false si no
 */
static bool is_valid_serial_port(uint16_t port) {
    return (port == SERIAL_COM1 ||
            port == SERIAL_COM2 ||
            port == SERIAL_COM3 ||
            port == SERIAL_COM4);
}

/**
 * @brief Verificar si el puerto serial existe (UART 16550+)
 * @param port Puerto a verificar
 * @return true si el puerto responde como UART, false si no
 * 
 * Verifica los bits 6-7 del IIR register que deben ser 1 en UART 16550+
 * Nota: En QEMU y algunos sistemas embebidos, esta verificación puede
 * no ser confiable. Se usa como optimización pero no es crítica.
 */
static bool serial_port_exists(uint16_t port) {
    /* 
     * UART 16550+ tiene bits 6-7 del IIR en 0xC0 cuando no hay interrupts
     * Sin embargo, algunos sistemas (QEMU, hardware antiguo) pueden devolver
     * otros valores. Usamos una verificación más permisiva:
     * - Leer IIR y verificar que no sea 0xFF (puerto inexistente)
     * - 0xFF típicamente indica puerto no presente (bus devuelve all-ones)
     */
    uint8_t iir = inb(port + SERIAL_IIR);
    
    /* 0xFF indica puerto inexistente (bus floating) */
    if (iir == 0xFF) {
        return false;
    }
    
    /* Cualquier otro valor indica puerto presente */
    return true;
}

/* ==========================================
 * Implementación de funciones públicas
 * ========================================== */

int serial_init(uint16_t port, uint32_t baud) {
    /* ==========================================
     * Validación de parámetros - CRÍTICO
     * Prevenir división por cero y I/O inválido
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

    /* Verificar que el puerto físicamente existe */
    if (!serial_port_exists(port)) {
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_INIT_FAIL;
        return 0;  // Port does not exist or is not a UART
    }

    /* Reset error state on successful init */
    serial_failed = 0;
    serial_error_code = SERIAL_ERROR_NONE;
    serial_timeout_count = 0;

    /* Guardar puerto */
    serial_port = port;

    /* Deshabilitar interrupciones */
    outb(port + SERIAL_IER, 0x00);

    /* Habilitar DLAB para configurar divisor de baud rate */
    outb(port + SERIAL_LCR, SERIAL_LCR_DLAB);

    /* Calcular divisor para el baud rate
     * Fórmula: divisor = 115200 / baud
     * Para 115200: divisor = 1
     * Para 9600: divisor = 12
     * Para 110: divisor = 1047
     * Nota: baud ya está validado en rango 110-115200
     */
    uint16_t divisor = 115200 / baud;
    outb(port + SERIAL_DLL, (divisor & 0xFF));       /* Low byte */
    outb(port + SERIAL_DLM, (divisor >> 8) & 0xFF);  /* High byte */

    /* Configurar 8 bits, no parity, 1 stop bit (8N1) y deshabilitar DLAB */
    outb(port + SERIAL_LCR, SERIAL_LCR_8N1);

    /* Habilitar FIFOs (16550), clear them, set 14 byte threshold */
    outb(port + SERIAL_FCR, 0x07);

    /* Configurar modem: DTR + RTS + OUT2 (enable interrupts) */
    outb(port + SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);

    /* Limpiar buffer de recepción leyendo cualquier dato pendiente */
    (void)inb(port + SERIAL_RBR);

    /* Pequeño delay para asegurar que el UART esté listo */
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
 * @brief Esperar hasta que se pueda escribir con timeout
 * @param timeout Número máximo de iteraciones (0 = SERIAL_MAX_WAIT)
 * @return true si el transmitter está vacío, false si timeout
 *
 * Usa busy-wait con límite para prevenir hangs infinitos
 */
static bool serial_wait_transmit_empty_timeout(uint32_t timeout) {
    if (!serial_initialized) {
        return false;
    }

    if (timeout == 0) {
        timeout = SERIAL_MAX_WAIT;
    }

    /* Esperar hasta que THRE (bit 5) esté set o timeout */
    while (timeout-- > 0) {
        if (inb(serial_port + SERIAL_LSR) & SERIAL_LSR_THRE) {
            return true;
        }
        /* Pequeño delay para evitar bus saturation */
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
    /* Wrapper que usa timeout por defecto */
    serial_wait_transmit_empty_timeout(SERIAL_MAX_WAIT);
}

void serial_write_char(char data) {
    /* Check if serial is initialized OR failed (for graceful degradation) */
    if (!serial_initialized) {
        return;  /* Serial not initialized or failed */
    }

    /* Esperar hasta que el transmitter holding register esté vacío */
    if (!serial_wait_transmit_empty_timeout(SERIAL_MAX_WAIT)) {
        /* Timeout occurred - hardware may have failed */
        /* Do NOT retry indefinitely - let caller decide what to do */
        return;
    }

    /* Escribir el caracter */
    outb(serial_port + SERIAL_THR, (uint8_t)data);
}

void serial_write_str(const char* str) {
    if (str == nullptr) {
        serial_failed = 1;
        serial_error_code = SERIAL_ERROR_NULL_PTR;
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
    char buffer[12];  /* Máximo 10 dígitos + null */
    int i = 10;

    buffer[11] = '\0';

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    serial_write_str(&buffer[i + 1]);
}

/**
 * @brief Escribir un entero de 64-bit en hexadecimal por serial
 * @param value Valor de 64-bit a escribir
 */
void serial_write_hex64(uint64_t value) {
    char buffer[19];  // "0x" + 16 digits + null = 19 bytes
    
    // Use shared utility function from hex_utils.h (DRY principle)
    uint64_to_hex_string(buffer, value);
    
    serial_write_str(buffer);
}

/**
 * @brief Escribir un entero de 64-bit en decimal por serial
 * @param value Valor de 64-bit a escribir
 */
void serial_write_dec64(uint64_t value) {
    char buffer[22];  // Máximo 20 dígitos + null
    int i = 20;

    buffer[21] = '\0';

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    serial_write_str(&buffer[i + 1]);
}

/**
 * @brief Escribir un entero de 32-bit en decimal con signo por serial
 * @param value Valor con signo a escribir
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
 * @brief Escribir un entero de 64-bit en decimal con signo por serial
 * @param value Valor de 64-bit con signo a escribir
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
    if (!serial_initialized || data == nullptr) return 0;

    /* Verificar si hay dato disponible (DR bit) */
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
